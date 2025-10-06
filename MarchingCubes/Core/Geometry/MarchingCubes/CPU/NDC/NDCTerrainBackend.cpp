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
#include <ppl.h>
#include <unordered_set>

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
		so.SetExecutionMode(ExecutionMode::ORT_PARALLEL);
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

void NDCTerrainBackend::requestBrush(const BrushRequest& req)
{
	Timer::BeginKey("NDCRequestBrush");
	RemeshRequest remeshRequest;
	remeshRequest.isoValue = req.isoValue;

	const XMUINT3 cells = m_gridDesc.cells;
	const XMFLOAT3 origin = m_gridDesc.origin;
	const float cellsize = m_gridDesc.cellsize;

	const float deltaTime = req.deltaTime;
	const XMFLOAT3 hitPos = req.hitpos;
	const float weight = req.weight;
	const float radius = req.radius;

	const int SX = int(cells.x) + 1;
	const int SY = int(cells.y) + 1;
	const int SZ = int(cells.z) + 1;

	const float kBase = std::clamp(m_brushDelta * deltaTime * std::abs(weight), 0.0f, 1.0f);

	// 영향 범위 (Field 인덱스 공간으로 변환)
	auto sample = [cellsize](float p, float o) { return (p - o) / cellsize; };
	XMINT3 min = {
		std::max(0, int(std::floor(sample(hitPos.x - radius, origin.x)))),
		std::max(0, int(std::floor(sample(hitPos.y - radius, origin.y)))),
		 std::max(0, int(std::floor(sample(hitPos.z - radius, origin.z))))
	};
	XMINT3 max = {
		std::min(SX - 1, int(std::ceil(sample(hitPos.x + radius, origin.x)))),
		std::min(SY - 1, int(std::ceil(sample(hitPos.y + radius, origin.y)))),
		std::min(SZ - 1, int(std::ceil(sample(hitPos.z + radius, origin.z))))
	};

	for (int z = min.z; z <= max.z; ++z)
	{
		const float pz = origin.z + z * cellsize;
		const float dz = pz - hitPos.z;

		for (int y = min.y; y <= max.y; ++y)
		{
			const float py = origin.y + y * cellsize;
			const float dy = py - hitPos.y;

			for (int x = min.x; x <= max.x; ++x)
			{
				const float px = origin.x + x * cellsize;
				const float dx = px - hitPos.x;

				const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
				if (dist > radius) continue; // 반경 밖은 영향 없음(빠른 스킵)

				// Brush 중심과의 거리에 따라 가중치 부여
				const float sphere = radius - dist;

				float& F = m_grd->F[z][y][x];
				float desired = (weight < 0) ? std::min(F, -sphere) : std::max(F, sphere);
				const float falloff = std::clamp(sphere / radius, 0.0f, 1.0f);
				const float k = kBase * falloff;

				F = F + (desired - F) * k;
			}
		}
	}

	// 1) 캐시 패치용 더티 AABB는 패딩(kPad=3)을 포함해서 확장 전달 (64³ 입력이 ±3 패딩을 포함하기 때문)
    XMINT3 dirtyMin = { 
		std::max(0, min.x - kPad), 
		std::max(0, min.y - kPad), 
		std::max(0, min.z - kPad) 
	};
    XMINT3 dirtyMax = { 
		std::min(SX-1, max.x + kPad), 
		std::min(SY-1, max.y + kPad), 
		std::min(SZ-1, max.z + kPad) 
	};
    NotifyBrushDirtyAABB(dirtyMin, dirtyMax);

    // 2) 리메시 대상 청크 범위 산출 (패딩 영향 고려해서 +kPad 확장한 AABB로 계산)
    const int step = kOutD - 1; // 57
    const auto chunksX = (int)((cells.x + step - 1) / step); // ceil
    const auto chunksY = (int)((cells.y + step - 1) / step);
    const auto chunksZ = (int)((cells.z + step - 1) / step);
    auto idxFloor = [&](int g)->int { return g / step; };                // g >= 0
    auto idxMax   = [&](int g)->int { return (std::max(0, g - 1)) / step; }; // 포함 상한: (gmax-1)/step
    int cx0 = std::clamp(idxFloor(dirtyMin.x), 0, std::max(0, chunksX - 1));
    int cy0 = std::clamp(idxFloor(dirtyMin.y), 0, std::max(0, chunksY - 1));
    int cz0 = std::clamp(idxFloor(dirtyMin.z), 0, std::max(0, chunksZ - 1));
    int cx1 = std::clamp(idxMax  (dirtyMax.x), 0, std::max(0, chunksX - 1));
    int cy1 = std::clamp(idxMax  (dirtyMax.y), 0, std::max(0, chunksY - 1));
    int cz1 = std::clamp(idxMax  (dirtyMax.z), 0, std::max(0, chunksZ - 1));
    if (cx1 < cx0 || cy1 < cy0 || cz1 < cz0) {
        // 더티가 너무 얇아 청크 경계에 걸리지 않은 케이스 → 최소 한 청크는 리메시 시킴
        cx0 = cx1 = std::clamp(idxFloor((dirtyMin.x+dirtyMax.x)/2), 0, std::max(0, chunksX-1));
        cy0 = cy1 = std::clamp(idxFloor((dirtyMin.y+dirtyMax.y)/2), 0, std::max(0, chunksY-1));
        cz0 = cz1 = std::clamp(idxFloor((dirtyMin.z+dirtyMax.z)/2), 0, std::max(0, chunksZ-1));
    }
    for (int cz = cz0; cz <= cz1; ++cz)
        for (int cy = cy0; cy <= cy1; ++cy)
            for (int cx = cx0; cx <= cx1; ++cx)
                remeshRequest.chunkset.insert(ChunkKey{ (uint32_t)cx, (uint32_t)cy, (uint32_t)cz });
 
	Timer::EndKey("NDCRequestBrush");
	requestRemesh(remeshRequest);
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
		Log::Print("NDC", "[Remesh] Full rebuild: req.chunkset empty");
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
	// Sliding Cache
	else
	{
		// 1) 이웃성 확보: (x,y,z) 오름차순 정렬
		std::vector<XMINT3> targets;
		targets.reserve(req.chunkset.size());
		for (auto& c : req.chunkset) {
			targets.push_back({
				static_cast<int>(c.x * m_chunkSize),
				static_cast<int>(c.y * m_chunkSize),
				static_cast<int>(c.z * m_chunkSize)
				});
		}
		std::sort(targets.begin(), targets.end(),
			[](const XMINT3& a, const XMINT3& b) {
				if (a.x != b.x) return a.x < b.x;
				if (a.y != b.y) return a.y < b.y;
				return a.z < b.z;
			});
		Log::Print("NDC", "[Remesh] Partial: chunkset=%u, step=%d", (unsigned)targets.size(), m_chunkSize);
		for (const auto& chunkStart : targets)
		{
			ChunkUpdate up;
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
	// 1) 입력 64^3 준비 (정점 SDF, 경계 클램프)
	std::vector<float> input;

	double buildTime = Timer::MeasureMs([&] {
		bool bSlid = buildInputSliding(chunkStart, input);
		Log::Print("NDC", "SlidingHit=%d  (cache=%d, prev=[%d,%d,%d], curr=[%d,%d,%d], step=%d)",
			(int)bSlid,(int)m_cacheValid, m_cachedStart.x, m_cachedStart.y, m_cachedStart.z,
			chunkStart.x, chunkStart.y, chunkStart.z, m_chunkSize);
		if (!bSlid)
		{
			buildInput(chunkStart, input);
			m_inputCache = input;
			m_cachedStart = chunkStart;
			m_cacheValid = true;
		}
		// 슬라이딩/동일 청크 재사용 이후, 브러시 더티 AABB가 있으면 그 부분만 빠르게 패치
		if (m_hasDirty) {
			patchCacaheFromDirtyAABB(chunkStart, input);
			// 캐시도 동기화
			m_inputCache = input;
		}
	});
	Log::Print("Timer", "buildInput : %.3f ms", buildTime);
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
		//outs = m_session.Run(Ort::RunOptions{ nullptr }, inNames, &inTensor, 1, outNames, 1);

		double sessionTime = Timer::MeasureMs([&] {
			outs = m_session.Run(Ort::RunOptions{ nullptr }, inNames, &inTensor, 1, outNames, 1);
			});
		Log::Print("Timer", "Session : %.3f ms", sessionTime);
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
	double DCTime = Timer::MeasureMs(&NDCTerrainBackend::DualContouringNDC, this, input.data(), Y, kOutD, kOutD, kOutD, outUpdate.md.vertices, outUpdate.md.indices, chunkOrigin, iso);
	Log::Print("Timer", "DualContouring : %.3f ms", DCTime);

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

bool NDCTerrainBackend::buildInputSliding(const DirectX::XMINT3& chunkStart, std::vector<float>& outData)
{
	if (!m_cacheValid || m_inputCache.size() != size_t(kInD * kInD * kInD))
		return false;

	const int step = m_chunkSize; // 보통 57 (요청 루프에서 이렇게 설정됨) :contentReference[oaicite:4]{index=4}
	const int dx = chunkStart.x - m_cachedStart.x;
	const int dy = chunkStart.y - m_cachedStart.y;
	const int dz = chunkStart.z - m_cachedStart.z;

    if (dx == 0 && dy == 0 && dz == 0) {
        outData.assign(m_inputCache.begin(), m_inputCache.end());
        return true;
    }

	// 단일 축으로만 이동한 경우만 처리 (대각선/멀리 점프는 전체 재구성)
	const bool movedXp = (dx == step) && dy == 0 && dz == 0;
	const bool movedXm = (dx == -step) && dy == 0 && dz == 0;
	const bool movedYp = (dy == step) && dx == 0 && dz == 0;
	const bool movedYm = (dy == -step) && dx == 0 && dz == 0;
	const bool movedZp = (dz == step) && dx == 0 && dy == 0;
	const bool movedZm = (dz == -step) && dx == 0 && dy == 0;
	const bool movedX = movedXp || movedXm;
	const bool movedY = movedYp || movedYm;
	const bool movedZ = movedZp || movedZm;
	if (!movedX && !movedY && !movedZ)
		return false;

	outData.resize(size_t(kInD * kInD * kInD));
	float* dst = outData.data();
	float* src = m_inputCache.data();

	// 유틸: 월드 그리드에서 TSDF 샘플 (기존 buildInput과 동일) :contentReference[oaicite:5]{index=5}
	const float trunc_vox = 100.0f;
	const float trunc_world = trunc_vox * m_gridDesc.cellsize; // :contentReference[oaicite:6]{index=6}
	const int Nxs = (int)m_gridDesc.cells.x;
	const int Nys = (int)m_gridDesc.cells.y;
	const int Nzs = (int)m_gridDesc.cells.z;                    // :contentReference[oaicite:7]{index=7}
	auto tsdfAt = [&](int gx, int gy, int gz) {
		gx = std::clamp(gx, 0, Nxs);
		gy = std::clamp(gy, 0, Nys);
		gz = std::clamp(gz, 0, Nzs);
		float s = m_grd->F[gz][gy][gx];
		return std::clamp(s / trunc_world, -1.0f, 1.0f);
		};

	const int overlap = kInD - step; // 64 - 57 = 7
	const size_t planeStride = size_t(kInD) * kInD; // 한 z-슬라이스 크기

	if (movedX) {
		// 1) [x=overlap..63] → [x=0..63-overlap] 슬라이딩 (각 y,z에 대해 행 단위 memmove)
        if (movedXp) {
            // → +X로 전진: [x=7..63] → [x=0..56], 오른쪽 7열 채움
            for (int z = 0; z < kInD; ++z) {
                for (int y = 0; y < kInD; ++y) {
                    float* rowDst = dst + (z * kInD + y) * kInD;
                    const float* rowSrc = src + (z * kInD + y) * kInD;
                    memmove(rowDst, rowSrc + overlap, sizeof(float) * (kInD - overlap));
                }
            }
            const int x0 = kInD - overlap; // 57..63 새로 채움
            for (int z = 0; z < kInD; ++z) {
                const int gzBase = chunkStart.z - kPad + z;
                for (int y = 0; y < kInD; ++y) {
                    const int gyBase = chunkStart.y - kPad + y;
                    for (int x = x0; x < kInD; ++x) {
                        const int gx = chunkStart.x - kPad + x;
                        dst[idx_inSDF(x, y, z)] = tsdfAt(gx, gyBase, gzBase);
                    }
                }
            }
        } else { // movedXm: -X로 후진
            // ← -X: [x=0..56] → [x=7..63], 왼쪽 7열 새로 채움
            for (int z = 0; z < kInD; ++z) {
                for (int y = 0; y < kInD; ++y) {
                    float* rowDst = dst + (z * kInD + y) * kInD;
                    const float* rowSrc = src + (z * kInD + y) * kInD;
                    memmove(rowDst + overlap, rowSrc, sizeof(float) * (kInD - overlap));
                }
            }
            const int x1 = overlap; // 0..6 새로 채움
            for (int z = 0; z < kInD; ++z) {
                const int gzBase = chunkStart.z - kPad + z;
                for (int y = 0; y < kInD; ++y) {
                    const int gyBase = chunkStart.y - kPad + y;
                    for (int x = 0; x < x1; ++x) {
                        const int gx = chunkStart.x - kPad + x;
                        dst[idx_inSDF(x, y, z)] = tsdfAt(gx, gyBase, gzBase);
                    }
                }
            }
        }
	}
	else if (movedY) {
		// 1) y축 슬라이딩: 각 z에 대해 [y=overlap..63] → [y=0..63-overlap]
        if (movedYp) {
            for (int z = 0; z < kInD; ++z) {
                float* planeDst = dst + z * planeStride;
                const float* planeSrc = src + z * planeStride;
                for (int y = 0; y < kInD - overlap; ++y) {
                    memmove(planeDst + y * kInD, planeSrc + (y + overlap) * kInD, sizeof(float) * kInD);
                }
            }
            const int y0 = kInD - overlap;
            for (int z = 0; z < kInD; ++z) {
                const int gzBase = chunkStart.z - kPad + z;
                for (int y = y0; y < kInD; ++y) {
                    const int gy = chunkStart.y - kPad + y;
                    for (int x = 0; x < kInD; ++x) {
                        const int gxBase = chunkStart.x - kPad + x;
                        dst[idx_inSDF(x, y, z)] = tsdfAt(gxBase, gy, gzBase);
                    }
                }
            }
        } 
		else 
		{ // movedYm
            for (int z = 0; z < kInD; ++z) 
			{
                float* planeDst = dst + z * planeStride;
                const float* planeSrc = src + z * planeStride;
                for (int y = 0; y < kInD - overlap; ++y) 
				{
                    memmove(planeDst + (y + overlap) * kInD, planeSrc + y * kInD, sizeof(float) * kInD);
                }
            }
            const int y1 = overlap;
            for (int z = 0; z < kInD; ++z) 
			{
                const int gzBase = chunkStart.z - kPad + z;
                for (int y = 0; y < y1; ++y) 
				{
                    const int gy = chunkStart.y - kPad + y;
                    for (int x = 0; x < kInD; ++x) 
					{
                        const int gxBase = chunkStart.x - kPad + x;
                        dst[idx_inSDF(x, y, z)] = tsdfAt(gxBase, gy, gzBase);
                    }
                }
            }
        }
	}
	else if (movedZ) 
	{
		if (movedZp) 
		{
			// +Z: [z=7..63] → [z=0..56]
			memmove(dst, src + overlap * planeStride, sizeof(float) * (kInD * kInD * (kInD - overlap)));
			const int z0 = kInD - overlap;
			for (int z = z0; z < kInD; ++z) 
			{
				const int gz = chunkStart.z - kPad + z;
				for (int y = 0; y < kInD; ++y) 
				{
					const int gyBase = chunkStart.y - kPad + y;
					for (int x = 0; x < kInD; ++x) {
						const int gxBase = chunkStart.x - kPad + x;
						dst[idx_inSDF(x, y, z)] = tsdfAt(gxBase, gyBase, gz);
					}
				}
			}
		}
		else { // movedZm
			// -Z: [z=0..56] → [z=7..63]
			memmove(dst + overlap * planeStride, src, sizeof(float) * (kInD * kInD * (kInD - overlap)));
			const int z1 = overlap;
			for (int z = 0; z < z1; ++z) {
				const int gz = chunkStart.z - kPad + z;
				for (int y = 0; y < kInD; ++y) {
					const int gyBase = chunkStart.y - kPad + y;
					for (int x = 0; x < kInD; ++x) {
						const int gxBase = chunkStart.x - kPad + x;
						dst[idx_inSDF(x, y, z)] = tsdfAt(gxBase, gyBase, gz);
					}
				}
			}
		}
	}
	// 새 캐시로 교체
	m_inputCache.swap(outData);
	outData.assign(m_inputCache.begin(), m_inputCache.end());
	m_cachedStart = chunkStart;
	m_cacheValid = true;
	return true;
}

void NDCTerrainBackend::NotifyBrushDirtyAABB(const DirectX::XMINT3& minG, const DirectX::XMINT3& maxG)
{
    // 여러 번 들어오면 누적 병합
    m_dirtyMin.x = std::min(m_dirtyMin.x, minG.x);
    m_dirtyMin.y = std::min(m_dirtyMin.y, minG.y);
    m_dirtyMin.z = std::min(m_dirtyMin.z, minG.z);
    m_dirtyMax.x = std::max(m_dirtyMax.x, maxG.x);
    m_dirtyMax.y = std::max(m_dirtyMax.y, maxG.y);
    m_dirtyMax.z = std::max(m_dirtyMax.z, maxG.z);
    m_hasDirty = true;
}

void NDCTerrainBackend::patchCacaheFromDirtyAABB(const DirectX::XMINT3& chunkStart, std::vector<float>& inoutData)
{
    if (!m_hasDirty) return;

    // 현재 64^3 입력 영역의 그리드 좌표 범위 (양끝 포함)
	const int gx0 = chunkStart.x - kPad;
	const int gy0 = chunkStart.y - kPad;
	const int gz0 = chunkStart.z - kPad;
    const int gx1 = gx0 + (kInD - 1);
    const int gy1 = gy0 + (kInD - 1);
    const int gz1 = gz0 + (kInD - 1);

    // 교집합(비어 있으면 곧장 반환)
    const int ix0 = std::max(gx0, m_dirtyMin.x);
    const int iy0 = std::max(gy0, m_dirtyMin.y);
    const int iz0 = std::max(gz0, m_dirtyMin.z);
    const int ix1 = std::min(gx1, m_dirtyMax.x);
    const int iy1 = std::min(gy1, m_dirtyMax.y);
    const int iz1 = std::min(gz1, m_dirtyMax.z);
    if (ix0 > ix1 || iy0 > iy1 || iz0 > iz1) {
        // 이번 64^3과는 겹치지 않음: 다음 기회로 미룸
        return;
    }

    // 유틸: TSDF 샘플 (buildInput과 동일)
    const float trunc_vox   = 100.0f;
    const float trunc_world = trunc_vox * m_gridDesc.cellsize;
    const int Nxs = (int)m_gridDesc.cells.x;
    const int Nys = (int)m_gridDesc.cells.y;
    const int Nzs = (int)m_gridDesc.cells.z;
    auto tsdfAt = [&](int gx, int gy, int gz) {
        gx = std::clamp(gx, 0, Nxs);
        gy = std::clamp(gy, 0, Nys);
        gz = std::clamp(gz, 0, Nzs);
        float s = m_grd->F[gz][gy][gx];
        return std::clamp(s / trunc_world, -1.0f, 1.0f);
    };

    // inoutData: 현재 64^3 입력 버퍼. 그리드 좌표(gx,gy,gz) → 로컬 인덱스(x,y,z) 변환
    for (int gz = iz0; gz <= iz1; ++gz) {
        const int z = gz - gz0;
        for (int gy = iy0; gy <= iy1; ++gy) {
            const int y = gy - gy0;
            for (int gx = ix0; gx <= ix1; ++gx) {
                const int x = gx - gx0;
                inoutData[idx_inSDF(x, y, z)] = tsdfAt(gx, gy, gz);
            }
        }
    }

    // 이번 패치를 소비했으니 더티 플래그 리셋
    m_dirtyMin = { INT_MAX, INT_MAX, INT_MAX };
    m_dirtyMax = { INT_MIN, INT_MIN, INT_MIN };
    m_hasDirty = false;
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