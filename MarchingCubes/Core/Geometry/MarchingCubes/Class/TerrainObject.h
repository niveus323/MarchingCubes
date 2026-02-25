#pragma once
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Geometry/MarchingCubes/Class/SdfDataComponent.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <DirectXCollision.h>
#include <unordered_map>
#include <set>

// Forward Declaration
class TerrainSystem;
class CustomMeshComponent;
class StaticMeshComponent;
class DataAsset;
class MaterialAsset;

/* [TerrainObject]
* - LifeTime : CreateObject -> Destroy
* - OwnerShip : Scene
* - Access : Scene::FindObject() / TerrainSystem을 통해 참조
* - Responsibility :
*	- Chunk Management : 거대한 지형을 논리적 청크 단위로 분할하여 관리
*	- Mesh Generation : TerrainSystem에 메쉬 생성을 요청하고, 결과물을 하위 컴포넌트(CustomMeshComponent)에 주입
* - Mesh Generation Flow : 
*	TerrainObject
*		├ SdfDataComponent로부터 메쉬 생성에 필요한 데이터를 받아옴
*		├ TerrainSystem에 데이터를 입력하여 메쉬를 받아옴
*		└ CustomMeshComponent에 메쉬 객체 전달
*/
class TerrainObject : public SceneObject
{
	REFLECT_GENERATED_BODY(TerrainObject)
public:
	struct ChunkInfo
	{
		CustomMeshComponent* meshComponent = nullptr;
	};

public:
	virtual void Init() override;
	virtual void Destroy() override;
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void OnPreSave() override;
	virtual void Serialize(Serializer& ar) override;

	std::shared_ptr<DataAsset> CreateDataAsset() const;
	bool SaveDataAsset(std::string_view path);
	bool LoadDataAsset(std::string_view path);
	void InjectTerrain(const GridDesc& desc, std::shared_ptr<SdfField> field);
	void ApplyBrush(float brushDelta, const DirectX::XMFLOAT3& pos, const float radius);
	void ApplyChunkMesh(ChunkKey key, std::vector<Vertex> vertices, std::vector<uint32_t> indices);

	void ShowChunkBounds(bool bShow = true);
	void ShowTerrainBound(bool bShow = true);
	void ShowWireframe(bool bShow = true);

	auto GetChunks() const { return m_chunks; }
	GridDesc GetSetting() const { return m_dataComponent->GetGridDesc(); }
	void SetSetting(const GridDesc& desc);
	std::shared_ptr<SdfField> GetData() const { return m_dataComponent->GetField(); }
	std::string_view GetAssetPath() const { return m_dataPath; }
	void SetAssetPath(const std::string& path) { m_dataPath = path; }
	bool IsDirty() const { return m_bDirty; }
	void SetDirty(bool bDirty) { m_bDirty = bDirty; }

protected:
	void BuildMesh(std::set<ChunkKey> chunks); // Data -> System 전달하여 Mesh 객체 저장

	friend class SdfDataComponent;
	void OnSettingUpdated();

private:
	std::set<ChunkKey> GetChunkKeys();

	bool IsKeyValid(const ChunkKey& key);

private:
	TerrainSystem* m_terrainSystem = nullptr;
	SdfDataComponent* m_dataComponent = nullptr;
	std::unordered_map<ChunkKey, ChunkInfo, ChunkKeyHash> m_chunks;
	std::string m_dataPath = "Unsaved";
	std::shared_ptr<MaterialAsset> m_material;
	bool m_bDirty = false;
	
#ifdef _DEBUG
	SceneObject* m_entireBoundingCube = nullptr;
	struct alignas(16) ChunkBoundaryConstants
	{
		DirectX::XMFLOAT3 gridOrigin = { 0.0f, 0.0f, 0.0f };
		float _padding0 = 0.0f;
		DirectX::XMFLOAT3 chunkWorldSize = { 50.0f, 50.0f,50.0f };
		float bias = 0.05f;
		DirectX::XMFLOAT4 lineColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	};
	BufferHandle m_chunkBorderCB;
	ChunkBoundaryConstants m_chunkBorderConstants;
	bool m_bShowChunkBounds = false;
#endif // _DEBUG
	// 지형 DataAsset의 헤더
	struct TerrainDataLayout
	{
		GridDesc desc;
	};
};