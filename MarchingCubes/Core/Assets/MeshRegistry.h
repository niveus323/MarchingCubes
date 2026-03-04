#pragma once
#include "Core/DataStructures/Data.h"
#include <unordered_map>
#include <string>

// Forward Declaration
class StaticMesh;
class DynamicMesh;
class MeshAsset;

/* [MeshRegistry]
* - LifeTime : RenderSystem Load -> RenderSystem UnLoad
* - OwnerShip : RenderSystem
* - Access : ResourceManager::GetMeshRegistry
* - Responsibility :
*   - Mesh Data Upload : GeometryData -> Buffer 업로드 및 Mesh 객체 관리
*/
class MeshRegistry
{
public:
    std::shared_ptr<StaticMesh> CreateStaticMesh(const std::string& key, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes);
    std::shared_ptr<StaticMesh> CreateStaticMesh(const std::shared_ptr<MeshAsset>& asset);
    std::shared_ptr<DynamicMesh> CreateDynamicMesh(const GeometryData& data, const std::vector<MeshSubmesh>& submeshes, const std::string& debugName = "");
    void UpdateDynamicMesh(std::shared_ptr<DynamicMesh> mesh, const GeometryData& newData, const std::vector<MeshSubmesh>& submeshes);

private:
    std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_resourceCache;
};

