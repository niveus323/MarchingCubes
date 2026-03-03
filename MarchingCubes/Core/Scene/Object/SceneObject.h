#pragma once
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Component/TransformComponent.h"

// Forward Declaration
class Scene;

/* [SceneObject]
* - Definition : Scene에 배치 가능한 오브젝트
* - LifeTime : Scene Load -> Scene UnLoad
* - Inheritence : GameObject
* - Access : Scene::GetSceneObject
* - Responsibility :
*	- TransformComponent : TransformComponent를 캐싱하여 관리
*	- Scene : 해당 오브젝트를 소유중인 Scene 참조
*/
class SceneObject : public GameObject, public GameObjectBase<SceneObject>
{
	REFLECT_GENERATED_BODY(SceneObject)
	friend class GameObjectBase<SceneObject>;
public:
	using GameObjectBase<SceneObject>::AddComponent;

	virtual ~SceneObject() = default;
	virtual void Init() override;
	virtual void Update(float deltaTime) override
	{
		GameObject::Update(deltaTime);
	}
	virtual void Render() override
	{
		for (auto& comp : GetComponents())
		{
			if(comp->IsActive()) comp->Submit();
		}
	}

	TransformComponent* GetTransformComponent() { return m_transformComp; }

	DirectX::XMMATRIX GetWorldMatrix() 
	{ 
		return (m_transformComp) ? 
			m_transformComp->GetWorldMatrix() : 
			DirectX::XMMatrixIdentity(); 
	}
	DirectX::XMFLOAT4X4 GetWorldTransform()
	{
		DirectX::XMFLOAT4X4 worldMatrix;
		DirectX::XMStoreFloat4x4(&worldMatrix, GetWorldMatrix());
		return worldMatrix;
	}
	DirectX::XMFLOAT3 GetPosition() const				{ return m_transformComp->GetPosition(); }
	DirectX::XMFLOAT3 GetWorldPosition() const			{ return m_transformComp->GetWorldPosition(); }
	void SetPosition(const DirectX::XMFLOAT3& pos)		{ m_transformComp->SetPosition(pos); }
	void SetPosition(float x, float y, float z)			{ m_transformComp->SetPosition({ x,y,z }); }
	void SetWorldPosition(const DirectX::XMFLOAT3& pos) { m_transformComp->SetWorldPosition(pos); }
	void SetWorldPosition(float x, float y, float z)	{ m_transformComp->SetWorldPosition({ x,y,z }); }
	DirectX::XMFLOAT3 GetRotation() const				{ return m_transformComp->GetRotation(); }
	DirectX::XMFLOAT3 GetWorldRotation() const			{ return m_transformComp->GetWorldRotation(); }
	void SetRotation(const DirectX::XMFLOAT3& rot)		{ m_transformComp->SetRotation(rot); }
	void SetRotation(const DirectX::XMVECTOR& quat)		{ m_transformComp->SetRotation(quat); }
	void SetWorldRotation(const DirectX::XMFLOAT3& rot) { m_transformComp->SetWorldRotation(rot); }
	void SetWorldRotation(const DirectX::XMVECTOR& quat){ m_transformComp->SetWorldRotation(quat); }
	DirectX::XMFLOAT3 GetScale() const					{ return m_transformComp->GetScale(); }
	DirectX::XMFLOAT3 GetWorldScale() const				{ return m_transformComp->GetWorldScale(); }
	void SetScale(const DirectX::XMFLOAT3& scale)		{ m_transformComp->SetScale(scale); }
	DirectX::XMVECTOR GetForwardVector() const			{ return m_transformComp->GetForwardVec(); }
	DirectX::XMVECTOR GetRightVector() const			{ return m_transformComp->GetRightVec(); }
	DirectX::XMVECTOR GetUpVector() const				{ return m_transformComp->GetUpVec(); }
	void SetTransform(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot, const DirectX::XMFLOAT3& scale) { m_transformComp->SetTransform(pos, rot, scale); }

private:
	TransformComponent* m_transformComp = nullptr;	

	// EditorOnly

};