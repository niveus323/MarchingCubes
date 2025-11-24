#pragma once
#include "ITerrainBackend.h"
#include <any>

// Forward Declaration
class RenderSystem;
class MeshChunkRenderer;
class DescriptorAllocator;
class UploadContext;

class TerrainSystem
{
public:
	struct InitInfo
	{
		ID3D12Device* device = nullptr;
		std::shared_ptr<SdfField<float>> grid;
		const GridDesc& desc;
		TerrainMode mode = TerrainMode::CPU_MC33;
		
		DescriptorAllocator* descriptorAllocator = nullptr;
		UploadContext* uploadContext = nullptr;
	};
public:
	explicit TerrainSystem(const InitInfo& info);
	TerrainSystem(ID3D12Device* device, std::shared_ptr<SdfField<float>> grid, const GridDesc& desc, TerrainMode mode);
	~TerrainSystem();

	void setMode(ID3D12Device* device, TerrainMode mode);
	void setGridDesc(ID3D12Device* deivce, const GridDesc& d);
	void setField(ID3D12Device* device, std::shared_ptr<SdfField<float>> grid);
	void requestRemesh(uint32_t frameIndex, const RemeshRequest& r);
	void requestRemesh(uint32_t frameIndex, float isoValue = 0.0f); // ÀüÃ¼ Remesh
	void requestBrush(uint32_t frameIndex, const BrushRequest& r);

	void tryFetch(ID3D12Device* device, RenderSystem* renderSystem, const std::string& psoName);

	// ChunkRenderer
	MeshChunkRenderer* GetRenderer() { return m_chunkRenderer.get(); }
	void ResetRenderer();

	//Debug
#ifdef _DEBUG
	void MakeDebugCell(GeometryData& outMeshData, bool bDrawFullCell);
	void EraseChunk(RenderSystem* renderSystem);
#endif // _DEBUG
	
private:
	TerrainMode				m_mode{ TerrainMode::GPU_ORIGINAL };
	std::shared_ptr<SdfField<float>>	m_lastGRD;
	GridDesc				m_desc{};

	DescriptorAllocator* m_descriptorAllocator = nullptr;
	UploadContext* m_uploadContext = nullptr;

	std::unique_ptr<ITerrainBackend> m_backend;
	std::unique_ptr<MeshChunkRenderer> m_chunkRenderer;
};

