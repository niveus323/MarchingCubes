#pragma once
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

class CPUTerrainBackend :   public ITerrainBackend
{
public:
	CPUTerrainBackend(ID3D12Device* device, const GridDesc& desc);

	// ITerrainBackend을(를) 통해 상속됨
	void setGridDesc(const GridDesc&) override;
	void setFieldPtr(std::shared_ptr<SdfField<float>> grid) override;
	void requestBrush(const BrushRequest&) override;
	bool tryFetch(std::vector<ChunkUpdate>& OutChunkUpdate) override;

protected:
	GridDesc m_gridDesc{};
	std::shared_ptr<SdfField<float>> m_grd;

	MeshData m_meshData;
	float m_brushDelta = 0.05f;
	UINT m_chunkSize;
};

