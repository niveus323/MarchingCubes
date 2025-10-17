#pragma once
#include "Core/Geometry/MarchingCubes/TerrainChunkRenderer.h"

class TerrainSystem
{
public:
	explicit TerrainSystem(ID3D12Device* device, std::shared_ptr<SdfField<float>> grd, const GridDesc& desc, TerrainMode mode);
	~TerrainSystem() = default;

	void setMode(ID3D12Device* device, TerrainMode mode);
	void setGridDesc(ID3D12Device* deivce, const GridDesc& d);
	
	void requestRemesh(const RemeshRequest& r);
	void requestBrush(const BrushRequest& r);

	void tryFetch(ID3D12Device* device, ID3D12Fence* graphicsFence, std::vector<ComPtr<ID3D12Resource>>* sink);

	// ChunkRenderer
	TerrainChunkRenderer* GetRenderer() { return m_chunkRenderer.get(); }
	void ResetRenderer() { m_chunkRenderer->Clear(); }
	void UploadRendererData(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, std::vector<std::pair<UINT64, UINT64>>& outAllocations);

	//Debug
	void MakeDebugCell(MeshData& outMeshData);

private:
	void initializeField(ID3D12Device* device, std::shared_ptr<SdfField<float>> grid, const GridDesc& desc);
	
private:
	TerrainMode				m_mode{ TerrainMode::GPU_ORIGINAL };
	std::shared_ptr<SdfField<float>>	m_lastGRD;
	GridDesc				m_desc{};

	std::unique_ptr<ITerrainBackend> m_backend;

	std::unique_ptr<TerrainChunkRenderer> m_chunkRenderer;
};

