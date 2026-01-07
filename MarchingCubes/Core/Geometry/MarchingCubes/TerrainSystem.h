#pragma once
#include "Core/Engine/Subsystem/SceneSubSystem.h"
#include "ITerrainBackend.h"

// Forward Declaration
class MeshChunkRenderer;
class DescriptorAllocator;
class UploadContext;

class TerrainSystem : public ISceneSubsystem
{
public:
	~TerrainSystem();
	virtual void Initialize() override;
	virtual void Update(float deltaTime) override;
	virtual void ExecuteCompute(uint32_t frameIndex) override;

	void LoadTerrain(TerrainMode mode, const GridDesc& desc, std::shared_ptr<SdfField<float>> field, const float isoValue = 0.0f);

	void SetMapData(const GridDesc& desc, std::shared_ptr<SdfField<float>> field);
	void SetMode(TerrainMode mode);
	void SetGridDesc(const GridDesc& d);
	void SetField(std::shared_ptr<SdfField<float>> field);
	void RequestRemesh(const std::set<ChunkKey>& chunkSet);
	void RequestRemesh(); // ÀüÃ¼ Remesh
	void RequestBrush(const BrushRequest& r);

	void tryFetch();

	// ChunkRenderer
	MeshChunkRenderer* GetRenderer() { return m_chunkRenderer.get(); }
	void ResetRenderer();

	//Debug
#ifdef _DEBUG
	void MakeDebugCell(GeometryData& outMeshData, bool bDrawFullCell);
#endif // _DEBUG
private:
	void RebuildBackend();

private:
	TerrainMode				m_mode{ TerrainMode::GPU_ORIGINAL };
	std::shared_ptr<SdfField<float>>	m_lastGRD;
	GridDesc				m_desc{};

	std::unique_ptr<ITerrainBackend> m_backend;
	std::unique_ptr<MeshChunkRenderer> m_chunkRenderer;
};

