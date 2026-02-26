#pragma once
#include "Core/Engine/Reflection.h"
#include "Core/Engine/Subsystem/SceneSubSystem.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <set>

// Forward Declaration
class MeshChunkRenderer;
class DescriptorAllocator;
class UploadContext;
class TerrainObject;


/* [TerrainSystem]
* - LifeTime : Scene Load (Initialize) -> Scene UnLoad (ShutDown)
* - OwnerShip : Scene (ISceneSubsystem)
* - Access : Scene::GetSubsystem<TerrainSystem>()
* - Responsibility :
*	- Mesh Generation : TerrainObject의 요청을 받아 SdfField를 GeometryData로 변환 (Marching Cubes 수행)
*	- Editor Tool Support : 브러시 충돌 검사(Raycast), 지형 수정 로직 등 에디터 기능을 위한 연산 제공
*	- Global Configuration : 지형 시스템의 전역 설정(LOD 정책, 물리 설정 등) 관리
*/
class TerrainSystem : public ISceneSubsystem
{
	REFLECT_GENERATED_BODY(TerrainSystem)
public:
	~TerrainSystem();
	virtual void Initialize() override;
	virtual void Update(float deltaTime) override;
	virtual void ExecuteCompute(uint32_t frameIndex) override;
	
	void RequestChunkRemesh(std::shared_ptr<TerrainObject> requester, const std::set<ChunkKey>& chunks);
	void SetMode(TerrainMode mode);

	//Debug
#ifdef _DEBUG
	static GeometryData MakeDebugCell(const GridDesc& desc, SdfField* field, bool bDrawAllCells = false);
#endif // _DEBUG
private:
	void RebuildBackend();

private:
	TerrainMode				m_mode{ TerrainMode::CPU_MC33 };
	std::unique_ptr<ITerrainBackend> m_backend;
};

