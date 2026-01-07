#pragma once
#include "RendererComponent.h"
#include "Core/DataStructures/Data.h"

class Mesh;
class SceneObject;

class MeshComponent : public RendererComponent
{
	REFLECT_GENERATED_BODY()
public:
	MeshComponent(SceneObject* owner);
	MeshComponent(SceneObject* owner, Mesh* mesh, std::string_view psoName);
	MeshComponent(SceneObject* owner, Mesh* mesh, const std::vector<std::string_view>& psoNames);
	virtual ~MeshComponent() = default;

	void SetMesh(Mesh* mesh);
	void SetMaterial(int slot, uint32_t materialHandle);
	void SetPSO(int slot, std::string_view psoName);
	void SetPSO(std::string_view psoName);
	virtual void Submit();

	// Mesh Path Accessor
	std::string GetMeshPath() const { return m_meshPath; }
	void SetMeshByPath(const std::string& path);

	// Material Accessor
	size_t GetMaterialSlotCount() const { return m_materials.size(); }
	std::string GetMaterialName(int slot) const;
	void SetMaterialByPath(int slot, const std::string& matPath);

private:
	// 런타임 렌더링용 캐시
	Mesh* m_mesh = nullptr; 
	std::vector<MaterialInstance> m_materials;
	
	// Serialization을 위해 에셋 경로로 관리
	std::string m_meshPath;
	std::vector<std::string> m_materialPaths;
};

