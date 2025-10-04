#include "pch.h"
#include "NDCTerrainBackend.h"
#include <algorithm>
#include <thread>
#include <windows.h>
#include <string>

NDCTerrainBackend::NDCTerrainBackend(const GridDesc& desc, std::shared_ptr<_GRD> grid) :
	CPUTerrainBackend(desc, grid),
	m_env(ORT_LOGGING_LEVEL_WARNING, "ndc"),
	m_session(nullptr)
{
	std::wstring onnxPath = GetFullPath(AssetType::Data, L"NDC/ndc_sdf_float.onnx");

	// onnx 모델 로드
	try {
		Ort::SessionOptions so;
		so.DisableMemPattern();
		so.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
		so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL); // 권장

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

	// 네트워크가 필요한 고정 코어로 분리. (입력이 64x64x64가 될 수 있도록 한다.)
	std::vector<float> sdfValues;
	int64_t D = 0, H = 0, W = 0;
	Ort::Value input;
	std::vector<Ort::Value> outputs;
	for (int x = 0; x < (int)m_gridDesc.cells.x; x += kOutD - 1)
	{
		for (int y = 0; y < (int)m_gridDesc.cells.y; y += kOutD - 1)
		{
			for (int z = 0; z < (int)m_gridDesc.cells.z; z += kOutD - 1)
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
	buildInput(chunkStart, input);

	// 2) ORT 텐서 만들기
	auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	std::array<int64_t, 5> ishape{ 1,1,kInD,kInD,kInD }; // N,C,D,H,W
	Ort::Value inTensor = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), ishape.data(), ishape.size());

	// 3) 추론
	const char* inNames[] = { m_inName.c_str() };
	const char* outNames[] = { m_outName.c_str() };
	std::vector<Ort::Value> outs;
	try
	{
		outs = m_session.Run(Ort::RunOptions{ nullptr }, inNames, &inTensor, 1, outNames, 1);
	}
	catch (const Ort::Exception& e) {
		OutputDebugStringA(("ORT FAIL: " + std::string(e.what()) + "\n").c_str());
	}
	assert(outs.size() == 1 && outs[0].IsTensor());
	const float* Y = outs[0].GetTensorData<float>(); // [1,3,64,64,64]

	// Chunk 시작 지점의 좌표 = 시작 인덱스 * 셀 크기
	XMFLOAT3 chunkOrigin = {
		m_gridDesc.origin.x + static_cast<float>(chunkStart.x) * m_gridDesc.cellsize,
		m_gridDesc.origin.y + static_cast<float>(chunkStart.y) * m_gridDesc.cellsize,
		m_gridDesc.origin.z + static_cast<float>(chunkStart.z) * m_gridDesc.cellsize
	};

	// 4) DC 재구성
	DualContouringNDC(input.data(), Y, kOutD, kOutD, kOutD, outUpdate.md.vertices, outUpdate.md.indices, chunkOrigin, m_gridDesc.cellsize, iso);

	outUpdate.empty = outUpdate.md.indices.empty();
	outUpdate.key.x = chunkStart.x / (kOutD - 1);
	outUpdate.key.y = chunkStart.y / (kOutD - 1);
	outUpdate.key.z = chunkStart.z / (kOutD - 1);

	return !outUpdate.empty;
}

// NDC 모델의 input은 [x,y,z] 배열. [z][y][x] -> [ ((x*NY) + y)*NZ + z ] 인덱싱 변환이 필요하다
void NDCTerrainBackend::buildInput(const DirectX::XMINT3& chunkStart, std::vector<float>& outData) const
{
	// cellsize: 월드 단위 한 셀 크기
	const float trunc_vox = 100.0f;                  // 3 voxels 권장
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
	const XMFLOAT3& origin, float cellsize, const float iso)
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

					float fx = std::clamp(float_grid[idx_output(i, j, k, 0)], 0.0f, 1.0f);
					float fy = std::clamp(float_grid[idx_output(i, j, k, 1)], 0.0f, 1.0f);
					float fz = std::clamp(float_grid[idx_output(i, j, k, 2)], 0.0f, 1.0f);

					// Output으로부터 Vertex 위치를 계산
					XMFLOAT3 outPos{
						origin.x + (static_cast<float>(i) + fx) * cellsize,
						origin.y + (static_cast<float>(j) + fy) * cellsize,
						origin.z + (static_cast<float>(k) + fz) * cellsize
					};

					Vertex v{};
					v.pos = outPos;
					v.normal = ComputeSdfNormal_Trilerp(input_sdf, dimX, dimY, dimZ, i, j, k, fx, fy, fz, cellsize);
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

DirectX::XMFLOAT3 NDCTerrainBackend::ComputeSdfNormal_Trilerp(
	const float* sdf, int dimX, int dimY, int dimZ,
	int i, int j, int k, float fx, float fy, float fz, float cellsize) const
{
	using namespace DirectX;

	auto sample = [&](int x, int y, int z) -> float {
		x = std::clamp(x, 0, dimX);
		y = std::clamp(y, 0, dimY);
		z = std::clamp(z, 0, dimZ);
		return sdf[idx_inSDF(x, y, z)];
		};

	const float inv2h = 1.0f / (2.0f * cellsize);
	auto gradAt = [&](int x, int y, int z) -> XMVECTOR {
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