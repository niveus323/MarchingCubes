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
	
	void LoadTerrain(TerrainMode mode, const GridDesc& desc, std::shared_ptr<SdfField> field, const float isoValue = 0.0f);
	bool IsLoaded() const { return m_desc.resolution.x > 0 && m_desc.cellsize > 0.0f; }

	void SetMapData(const GridDesc& desc, std::shared_ptr<SdfField> field);
	void SetMode(TerrainMode mode);
	void SetGridDesc(const GridDesc& d);
	const GridDesc& GetGridDesc() const { return m_desc; }
	void SetField(std::shared_ptr<SdfField> field);
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
	TerrainMode				m_mode{ TerrainMode::CPU_MC33 };
	std::shared_ptr<SdfField>	m_lastGRD;
	GridDesc				m_desc{};

	std::unique_ptr<ITerrainBackend> m_backend;
	std::unique_ptr<MeshChunkRenderer> m_chunkRenderer;
};

