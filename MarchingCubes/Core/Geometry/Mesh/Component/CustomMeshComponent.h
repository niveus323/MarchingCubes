#pragma once
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/DataStructures/Data.h"
#include "Core/Geometry/Mesh/Class/DynamicMesh.h"

/* [CustomMeshComponent]
* - LifeTime : AddComponent -> RemoveComponent / Object Destroy
* - OwnerShip : SceneObject 
* - Access : GameObject::GetComponent
* - Responsibility :
*	- CPU Data : GeometryData 包府 棺 悼利 Update
*	- GPU Data : DynamicMesh 按眉 包府 棺 力傍
*/

class CustomMeshComponent : public MeshComponent
{
	REFLECT_GENERATED_BODY(CustomMeshComponent)
public:
	virtual void Serialize(Serializer& ar) override;
	virtual Mesh* GetMesh() const override { return m_mesh.get(); }
	const GeometryData& GetData() const { return m_data; }
	void UpdateMesh(GeometryData data, const std::vector<MeshSubmesh>& submeshes);

private:
	std::shared_ptr<DynamicMesh> m_mesh;
	GeometryData m_data;
};

