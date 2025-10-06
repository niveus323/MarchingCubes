#include "pch.h"
#include "NDCTerrainBackend.h"
#include <algorithm>
#include <thread>
#include <windows.h>
#include <string>
//#include "cpu_provider_factory.h"
#include <dml_provider_factory.h>
#include <onnxruntime_c_api.h>

#include "Core/Utils/Timer.h"
#include "Core/Utils/Log.h"

NDCTerrainBackend::NDCTerrainBackend(ID3D12Device* device, const GridDesc& desc) :
	CPUTerrainBackend(device, desc),
	m_env(ORT_LOGGING_LEVEL_WARNING, "ndc"),
	m_session(nullptr)
{
	m_chunkSize = kInD - kPad - 1;
	std::wstring onnxPath = GetFullPath(AssetType::Data, L"NDC/ndc_sdf_float.onnx");

	// onnx 모델 로드
	try {
		OrtApi const& ortApi = Ort::GetApi();
		OrtDmlApi const* ortDmlApi = nullptr;
		ortApi.GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<void const**>(&ortDmlApi));

		Ort::SessionOptions so;
		so.DisableMemPattern();
		so.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
		so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

		// 외장 GPU를 사용하지 않으므로 0번 디바이스를 사용 (TODO : 외장 GPU 사용 여부 체크)
		ortDmlApi->SessionOptionsAppendExecutionProvider_DML(so, 0);

		// NOTE : ONNX + DML 사용 시 MetaCommand 생성을 시도, 실패 시 다른 방식으로 Fallback 한다. 이 과정에서 META_COMMAND_UNSUPPORTED_PARAMS 메시지가 초기화 시 발생
		// 직접 DirectML을 사용하여 MetaCommand 생성을 시도하지 않는 방식으로 메시지 처리 가능하나 이미 이 과정으로도 모델 아웃풋이 잘 나오기 때문에 에러를 무시하고 작업함
		m_session = Ort::Session(m_env, onnxPath.c_str(), so);
	}
	catch (const Ort::Exception& e) {
		OutputDebugStringA(("ORT FAIL: " + std::string(e.what()) + "\n").c_str());
	}

	// 입/출력 이름 조회 (0번만 사용)
	Ort::AllocatorWithDefaultOptions alloc;
	{
		auto in_name = m_session.GetInputNameAllocated(0, alloc);
		auto out_name = m_session.GetOutputNameAllocated(0, alloc);
		m_inName = in_name.get();
		m_outName = out_name.get();
	}
}

// onnx 모델 추론
void NDCTerrainBackend::requestRemesh(const RemeshRequest& req)
{
	m_chunkUpdates.clear();

	// Seam 처리를 위해 앞 셸 3개 (포인트 4개)는 padding으로서 이전 chunk의 셸 3개를 포함한다. 실질적으로 이번 chunk에서 처리하는 크기는 64 -3x2 -1 = 57
	m_chunkSize = kOutD - 1;

	// 네트워크가 필요한 고정 코어로 분리. (입력이 64x64x64가 될 수 있도록 한다.)
	std::vector<float> sdfValues;
	int64_t D = 0, H = 0, W = 0;
	Ort::Value input;
	std::vector<Ort::Value> outputs;

	// 갱신할 메쉬의 chunk 정보가 없을 경우 전체 갱신
	if (req.chunkset.empty())
	{
		for (int x = 0; x < (int)m_gridDesc.cells.x; x += m_chunkSize)
		{
			for (int y = 0; y < (int)m_gridDesc.cells.y; y += m_chunkSize)
			{
				for (int z = 0; z < (int)m_gridDesc.cells.z; z += m_chunkSize)
				{
					ChunkUpdate up;
					if (BuildNdcInputFromGRD(req.isoValue, XMINT3(x, y, z), up))
					{
						m_chunkUpdates.push_back(up);
					}
				}
			}
		}
	}
	else
	{
		for (auto& chunk : req.chunkset)
		{
			ChunkUpdate up;
			XMINT3 chunkStart = {
				static_cast<int>(chunk.x * m_chunkSize),
				static_cast<int>(chunk.y * m_chunkSize),
				static_cast<int>(chunk.z * m_chunkSize)
			};
			if (BuildNdcInputFromGRD(req.isoValue, chunkStart, up))
			{
				m_chunkUpdates.push_back(up);
			}
		}
	}
}

bool NDCTerrainBackend::tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates)
{
	OutChunkUpdates = std::move(m_chunkUpdates);
	m_chunkUpdates.clear();
	return !OutChunkUpdates.empty();
}

bool NDCTerrainBackend::BuildNdcInputFromGRD(const float iso, const DirectX::XMINT3& chunkStart, ChunkUpdate& outUpdate)
{
	// 1) 입력 70^3 준비 (정점 SDF, 경계 클램프)
	std::vector<float> input;

	double buildInputTime = Timer::MeasureMs(&NDCTerrainBackend::buildInput, this, chunkStart, input);
	Log::Print("Timer", "[NDC BuildInput] %.3f", buildInputTime);
	//buildInput(chunkStart, input);

	// 2) ORT 텐서 만들기
	auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	std::array<int64_t, 5> ishape{ 1,1,kInD,kInD,kInD }; // N(1),C(1),D(64),H(64),W(64)
	Ort::Value inTensor = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), ishape.data(), ishape.size());

	// 3) 추론
	const char* inNames[] = { m_inName.c_str() };
	const char* outNames[] = { m_outName.c_str() };
	std::vector<Ort::Value> outs;
	try
	{
		Timer::BeginKey("NDCRun");
		outs = m_session.Run(Ort::RunOptions{ nullptr }, inNames, &inTensor, 1, outNames, 1);
		double sessionTime = Timer::EndKey("NDCRun");
		Log::Print("Timer", "[NDC SessionRun] %.3f", sessionTime);
	}
	catch (const Ort::Exception& e) {
		OutputDebugStringA(("ORT FAIL: " + std::string(e.what()) + "\n").c_str());
	}
	assert(outs.size() == 1 && outs[0].IsTensor());
	const float* Y = outs[0].GetTensorData<float>(); // [1,3,64,64,64]

	// Chunk 시작 지점의 좌표 = 시작 인덱스 * 셀 크기
	XMFLOAT3 chunkOrigin = {
		m_gridDesc.origin.x + static_cast<float>(chunkStart.x - kPad) * m_gridDesc.cellsize,
		m_gridDesc.origin.y + static_cast<float>(chunkStart.y - kPad) * m_gridDesc.cellsize,
		m_gridDesc.origin.z + static_cast<float>(chunkStart.z - kPad) * m_gridDesc.cellsize
	};

	// 4) DC 재구성
	double dcTime = Timer::MeasureMs(&NDCTerrainBackend::DualContouringNDC, this,input.data(), Y, kOutD, kOutD, kOutD, outUpdate.md.vertices, outUpdate.md.indices, chunkOrigin, iso);
	//DualContouringNDC(input.data(), Y, kOutD, kOutD, kOutD, outUpdate.md.vertices, outUpdate.md.indices, chunkOrigin, iso);

	outUpdate.empty = outUpdate.md.indices.empty();
	outUpdate.key.x = chunkStart.x / m_chunkSize;
	outUpdate.key.y = chunkStart.y / m_chunkSize;
	outUpdate.key.z = chunkStart.z / m_chunkSize;

	return !outUpdate.empty;
}

// NDC 모델의 input은 [x,y,z] 배열. [z][y][x] -> [ ((x*NY) + y)*NZ + z ] 인덱싱 변환이 필요하다
void NDCTerrainBackend::buildInput(const DirectX::XMINT3& chunkStart, std::vector<float>& outData) const
{
	// cellsize: 월드 단위 한 셀 크기
	const float trunc_vox = 100.0f;
	const float trunc_world = trunc_vox * m_gridDesc.cellsize;

	const int Nxs = (int)m_gridDesc.cells.x;
	const int Nys = (int)m_gridDesc.cells.y;
	const int Nzs = (int)m_gridDesc.cells.z;

	outData.resize(kInD * kInD * kInD);
	for (int z = 0; z < kInD; ++z)
	{
		int gz = std::clamp(chunkStart.z - kPad + z, 0, Nzs);
		for (int y = 0; y < kInD; ++y)
		{
			int gy = std::clamp(chunkStart.y - kPad + y, 0, Nys);
			for (int x = 0; x < kInD; ++x)
			{
				int gx = std::clamp(chunkStart.x - kPad + x, 0, Nxs);

				float s = m_grd->F[gz][gy][gx];                // 월드 단위 거리(SDF)
				float tsdf = std::clamp(s / trunc_world, -1.0f, 1.0f);
				outData[idx_inSDF(x, y, z)] = tsdf;
			}
		}
	}

}

void NDCTerrainBackend::ComputeVertexNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, bool areaWeighted) const
{
	std::vector<XMVECTOR> acc(vertices.size());

	// 1) 삼각형 루프
	const size_t triCount = indices.size() / 3;
	for (size_t t = 0; t < triCount; ++t) {
		uint32_t i0 = indices[t * 3 + 0];
		uint32_t i1 = indices[t * 3 + 1];
		uint32_t i2 = indices[t * 3 + 2];
		if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
			continue; // 방어

		const XMVECTOR p0 = XMLoadFloat3(&vertices[i0].pos);
		const XMVECTOR p1 = XMLoadFloat3(&vertices[i1].pos);
		const XMVECTOR p2 = XMLoadFloat3(&vertices[i2].pos);

		// 에지 벡터
		XMVECTOR e0 = p1 - p0;
		XMVECTOR e1 = p2 - p0;

		// 면 노말(크기는 면적*2) = cross(e0, e1)
		XMVECTOR n = XMVector3Cross(e0, e1);

		// 면적 가중 X(=모든 면 동일 가중) 옵션이면 단위화 후 누적
		if (!areaWeighted) {
			float len2 = XMVectorGetX(XMVector3LengthSq(n));
			if (len2 > 1e-20f) n = XMVector3Normalize(n);
		}

		// 누적
		acc[i0] += n;
		acc[i1] += n;
		acc[i2] += n;
	}

	// 2) 정규화해서 vertices[].normal에 기록
	for (size_t i = 0; i < vertices.size(); ++i) {
		XMVector3Normalize(acc[i]);
		XMStoreFloat3(&vertices[i].normal, acc[i]);
	}
}

// float_grid: shape [dimX,dimY,dimZ,3], row-major (마지막 축 연속)
void NDCTerrainBackend::DualContouringNDC(const float* input_sdf, const float* float_grid,
	int dimX, int dimY, int dimZ,
	std::vector<Vertex>& outVertices,
	std::vector<uint32_t>& outIndices,
	const XMFLOAT3& origin, const float iso)
{
	if (dimX <= 0 || dimY <= 0 || dimZ <= 0) {
		outVertices.clear(); outIndices.clear(); return;
	}

	// (3, dimZ, dimY, dimX)
	auto idx_output = [dimX, dimY, dimZ](int x, int y, int z, int c) -> size_t {
		return (((size_t)c * dimZ + z) * dimY + y) * dimX + x;
		};

	auto idx_YZ = [dimY, dimZ](int y, int z) -> size_t {
		return (size_t)y * dimZ + z;
		};

	// 파이썬은 초기 capacity 16384에서 2배씩 증가하지만,
	// C++에선 reserve만 해두고 push_back으로 동적 증가
	outVertices.clear();
	outIndices.clear();
	outVertices.reserve(16384);
	outIndices.reserve(16384 * 3);

	// x 순회를 돌면서 x평면으로 버텍스를 저장하는 컨테이너를 둔다. (y, z 평면은 prevPlane(x = i-1 평면)의 버텍스와 연결하여 형성)
	std::vector<size_t> prevPlane((size_t)dimY * dimZ, -1);
	std::vector<size_t> currPlane((size_t)dimY * dimZ, -1);

	// 각 셀의 정점을 생성 및 평면 형성
	for (int i = 0; i < dimX; ++i) {
		if (i > 0)
		{
			std::swap(prevPlane, currPlane);
			std::fill(currPlane.begin(), currPlane.end(), -1);
		}

		for (int j = 0; j < dimY; ++j) {
			for (int k = 0; k < dimZ; ++k) {
				bool v0 = input_sdf[idx_inSDF(i, j, k)] < iso;
				bool v1 = input_sdf[idx_inSDF(i + 1, j, k)] < iso;
				bool v2 = input_sdf[idx_inSDF(i + 1, j + 1, k)] < iso;
				bool v3 = input_sdf[idx_inSDF(i, j + 1, k)] < iso;
				bool v4 = input_sdf[idx_inSDF(i, j, k + 1)] < iso;
				bool v5 = input_sdf[idx_inSDF(i + 1, j, k + 1)] < iso;
				bool v6 = input_sdf[idx_inSDF(i + 1, j + 1, k + 1)] < iso;
				bool v7 = input_sdf[idx_inSDF(i, j + 1, k + 1)] < iso;

				// 8방향에 대해 표면 체크
				if (v1 != v0 || v2 != v0 || v3 != v0 || v4 != v0 || v5 != v0 || v6 != v0 || v7 != v0) 
				{
					currPlane[idx_YZ(j, k)] = outVertices.size();

					// SDF 포인트로부터 얼마나 떨어진 위치에 정점을 생성해야하는가
					float fx = std::clamp(float_grid[idx_output(i, j, k, 0)], 0.0f, 1.0f);
					float fy = std::clamp(float_grid[idx_output(i, j, k, 1)], 0.0f, 1.0f);
					float fz = std::clamp(float_grid[idx_output(i, j, k, 2)], 0.0f, 1.0f);

					// Output으로부터 Vertex 위치를 계산
					XMFLOAT3 outPos{
						origin.x + (static_cast<float>(i) + fx) * m_gridDesc.cellsize,
						origin.y + (static_cast<float>(j) + fy) * m_gridDesc.cellsize,
						origin.z + (static_cast<float>(k) + fz) * m_gridDesc.cellsize
					};

					Vertex v{};
					v.pos = outPos;
					v.normal = ComputeSdfNormal_Trilerp(input_sdf, dimX, dimY, dimZ, i, j, k, fx, fy, fz);
					v.color = { 1.0f, 1.0f, 1.0f, 1.0f };

					outVertices.push_back(v);
				}

				/*
				* Quad 생성
				*/

				// X - Plane
				if ((j > 0 && k > 0) && (v0 ^ v1))
				{
					const uint32_t a = (uint32_t)currPlane[idx_YZ(j - 1, k - 1)];
					const uint32_t b = (uint32_t)currPlane[idx_YZ(j, k)];
					const uint32_t c = (uint32_t)currPlane[idx_YZ(j, k - 1)];
					const uint32_t d = (uint32_t)currPlane[idx_YZ(j - 1, k)];
					if (!v0)
					{
						outIndices.insert(outIndices.end(), { a,c,b, a,b,d });
					}
					else
					{
						outIndices.insert(outIndices.end(), { a,b,c, a,d,b, });
					}
				}

				// Y - Plane
				if ((i > 0 && k > 0) && (v0 ^ v3))
				{
					const uint32_t a = (uint32_t)prevPlane[idx_YZ(j, k - 1)]; // (i-1,j,k-1)
					const uint32_t b = (uint32_t)prevPlane[idx_YZ(j, k)]; //(i-1, j, k)
					const uint32_t c = (uint32_t)currPlane[idx_YZ(j, k - 1)]; // (i, j, k-1)
					const uint32_t d = (uint32_t)currPlane[idx_YZ(j, k)]; //(i, j, k)

					if (!v0)
					{
						outIndices.insert(outIndices.end(), { a, d, c,  a, b, d });
					}
					else
					{
						outIndices.insert(outIndices.end(), { a, c, d, a, d, b });
					}

				}

				// Z - Plane
				if ((i > 0 && j > 0) && (v0 ^ v4))
				{
					const uint32_t a = (uint32_t)prevPlane[idx_YZ(j - 1, k)]; // (i-1, j-1, k)
					const uint32_t b = (uint32_t)prevPlane[idx_YZ(j, k)]; // (i-1, j, k)
					const uint32_t c = (uint32_t)currPlane[idx_YZ(j - 1, k)]; // (i, j-1, k)
					const uint32_t d = (uint32_t)currPlane[idx_YZ(j, k)]; // (i, j, k)

					if (!v0) {
						outIndices.insert(outIndices.end(), { a, d, b,  a, c, d });
					}
					else {
						outIndices.insert(outIndices.end(), { a, b, d,  a, d, c });
					}
				}
			}
		}
	}
}

DirectX::XMFLOAT3 NDCTerrainBackend::ComputeSdfNormal_Trilerp(const float* sdf, int dimX, int dimY, int dimZ,
	int i, int j, int k, float fx, float fy, float fz) const
{
	using namespace DirectX;

	auto sample = [sdf, dimX, dimY, dimZ](int x, int y, int z) -> float {
		x = std::clamp(x, 0, dimX);
		y = std::clamp(y, 0, dimY);
		z = std::clamp(z, 0, dimZ);
		return sdf[idx_inSDF(x, y, z)];
		};

	const float inv2h = 1.0f / (2.0f * m_gridDesc.cellsize);
	auto gradAt = [sample, inv2h](int x, int y, int z) -> XMVECTOR {
		float gx = (sample(x + 1, y, z) - sample(x - 1, y, z)) * inv2h;
		float gy = (sample(x, y + 1, z) - sample(x, y - 1, z)) * inv2h;
		float gz = (sample(x, y, z + 1) - sample(x, y, z - 1)) * inv2h;
		return XMVectorSet(gx, gy, gz, 0.0f);
		};

	// 코너 8점의 ∇S
	XMVECTOR g000 = gradAt(i, j, k);
	XMVECTOR g100 = gradAt(i + 1, j, k);
	XMVECTOR g110 = gradAt(i + 1, j + 1, k);
	XMVECTOR g010 = gradAt(i, j + 1, k);
	XMVECTOR g001 = gradAt(i, j, k + 1);
	XMVECTOR g101 = gradAt(i + 1, j, k + 1);
	XMVECTOR g111 = gradAt(i + 1, j + 1, k + 1);
	XMVECTOR g011 = gradAt(i, j + 1, k + 1);

	// 삼선형 보간: tx=fx, ty=fy, tz=fz
	XMVECTOR g00 = XMVectorLerp(g000, g100, fx);
	XMVECTOR g10 = XMVectorLerp(g010, g110, fx);
	XMVECTOR g01 = XMVectorLerp(g001, g101, fx);
	XMVECTOR g11 = XMVectorLerp(g011, g111, fx);
	XMVECTOR g0 = XMVectorLerp(g00, g10, fy);
	XMVECTOR g1 = XMVectorLerp(g01, g11, fy);
	XMVECTOR g = XMVectorLerp(g0, g1, fz);

	// 외부로 향하도록 부호 조정 후 정규화
	g = XMVectorScale(g, -1.0f);
	XMVECTOR len2 = XMVector3LengthSq(g);
	if (XMVectorGetX(len2) < 1e-14f) { // 평탄/불안정 보호
		g = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}
	else {
		g = XMVector3Normalize(g);
	}

	XMFLOAT3 n;
	XMStoreFloat3(&n, g);
	return n;
}