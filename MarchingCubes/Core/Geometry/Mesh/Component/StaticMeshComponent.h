#pragma once
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Geometry/Mesh/Class/StaticMesh.h"

// Forward Declaration
class MeshAsset;

/* [StaticMeshComponent]
* - LifeTime : AddComponent -> RemoveComponent / Object Destroy
* - OwnerShip : SceneObject
* - Access : GameObject::GetComponent
* - Responsibility :
*	- GPU Data : StaticMesh 객체 관리 및 제공
*	- Mesh Asset Referencing : MeshAsset을 참조하여 CPU 데이터 설정
*/
class StaticMeshComponent : public MeshComponent
{
	REFLECT_GENERATED_BODY(StaticMeshComponent)
public:
	void Serialize(Serializer& ar) override;

	// Mesh Accessor
	virtual Mesh* GetMesh() const override { return m_mesh.get(); }
	const std::string& GetMeshPath() const { return m_path; }
	void SetMeshByPath(const std::string& path);

private:
	std::shared_ptr<StaticMesh> m_mesh;
	std::shared_ptr<MeshAsset> m_meshAsset;
	std::string m_path;
};

