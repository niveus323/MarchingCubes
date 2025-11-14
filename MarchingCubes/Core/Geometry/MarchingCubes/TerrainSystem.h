#pragma once
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"

class RenderSystem;

class TerrainSystem
{
public:
	explicit TerrainSystem(ID3D12Device* device, std::shared_ptr<SdfField<float>> grd, const GridDesc& desc, TerrainMode mode);
	~TerrainSystem() = default;

	void setMode(ID3D12Device* device, TerrainMode mode);
	void setGridDesc(ID3D12Device* deivce, const GridDesc& d);
	void setField(ID3D12Device* device, std::shared_ptr<SdfField<float>> grid);
	void requestRemesh(const RemeshRequest& r);
	void requestRemesh(float isoValue = 0.0f); // ÀüÃ¼ Remesh
	void requestBrush(const BrushRequest& r);

	void tryFetch(ID3D12Device* device, RenderSystem* renderSystem, const std::string& psoName);

	// ChunkRenderer
	MeshChunkRenderer* GetRenderer() { return m_chunkRenderer.get(); }
	void ResetRenderer() { m_chunkRenderer->Clear(); }

	//Debug
#ifdef _DEBUG
	void MakeDebugCell(GeometryData& outMeshData, bool bDrawFullCell);
	void EraseChunk(RenderSystem* renderSystem);
#endif // _DEBUG
	
private:
	TerrainMode				m_mode{ TerrainMode::GPU_ORIGINAL };
	std::shared_ptr<SdfField<float>>	m_lastGRD;
	GridDesc				m_desc{};

	std::unique_ptr<ITerrainBackend> m_backend;

	std::unique_ptr<MeshChunkRenderer> m_chunkRenderer;
};

