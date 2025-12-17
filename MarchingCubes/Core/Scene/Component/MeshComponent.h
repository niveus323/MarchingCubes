#pragma once
#include "RendererComponent.h"
#include "Core/DataStructures/Data.h"

class Mesh;
class SceneObject;

class MeshComponent : public RendererComponent
{
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

private:
	Mesh* m_mesh = nullptr; 
	std::vector<MaterialInstance> m_materials;
};

