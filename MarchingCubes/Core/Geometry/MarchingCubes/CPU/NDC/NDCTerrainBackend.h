#pragma once
#include "Core/Geometry/MarchingCubes/CPU/CPUTerrainBackend.h"
#include "onnxruntime_cxx_api.h"

class NDCTerrainBackend : public CPUTerrainBackend
{
public:
	NDCTerrainBackend(ID3D12Device* device, const GridDesc& desc);
	
	// ITerrainBackend을(를) 통해 상속됨
	void requestBrush(const BrushRequest&) override;
	void requestRemesh(const RemeshRequest& req) override;
	bool tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates) override;

private:
	bool BuildNdcInputFromGRD(const float iso, const DirectX::XMINT3& chunkStart, ChunkUpdate& outUpdate);
	void buildInput(const DirectX::XMINT3& chunkStart, std::vector<float>& outData) const;
	bool buildInputSliding(const DirectX::XMINT3& chunkStart, std::vector<float>& outData);
	void NotifyBrushDirtyAABB(const DirectX::XMINT3& minG, const DirectX::XMINT3& maxG);
	void patchCacaheFromDirtyAABB(const DirectX::XMINT3& chunkStart, std::vector<float>& inoutData);
	void ComputeVertexNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, bool areaWeighted = true) const;
	void DualContouringNDC(const float* input_sdf, const float* float_grid, int dimX, int dimY, int dimZ, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, const XMFLOAT3& origin = { 0,0,0 }, const float iso = 0.5f);
	DirectX::XMFLOAT3 ComputeSdfNormal_Trilerp(const float* sdf, int dimX, int dimY, int dimZ, int i, int j, int k, float fx, float fy, float fz) const;
	// util: shape 값이 전부 비동적(>=0)인지
	static bool allFinite(const std::vector<int64_t>& v) {
		for (auto d : v) if (d < 0) return false;
		return true;
	}

	inline static size_t idx_inSDF(int x, int y, int z)
	{
		return static_cast<size_t>(((z * kInD) + y) * kInD + x);
	}

private:
	// 고정 규약
	static constexpr int kInD = 64; 
	static constexpr int kOutD = 58; // = 64 - 6 (pad ±3)
	static constexpr int kPad = 3;

	std::vector<ChunkUpdate> m_chunkUpdates;

    Ort::Env m_env;
    Ort::Session m_session;
    std::string m_inName, m_outName;

	// 슬라이딩 윈도우 캐시
	mutable std::vector<float> m_inputCache;    // 최근 64^3 입력 (tsdf)
	mutable DirectX::XMINT3    m_cachedStart{ INT_MIN, INT_MIN, INT_MIN }; // 캐시 기준 chunkStart
	mutable bool               m_cacheValid{ false };

	// 브러시로 수정된 그리드 AABB(그리드 인덱스 좌표계)
	mutable DirectX::XMINT3    m_dirtyMin{ INT_MAX, INT_MAX, INT_MAX };
	mutable DirectX::XMINT3    m_dirtyMax{ INT_MIN, INT_MIN, INT_MIN };
	mutable bool               m_hasDirty{ false };
};

