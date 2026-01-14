#pragma once
#include "Component.h"
#include "Core/DataStructures/Data.h"

// Forward Delcaration
class GameObject;

/* [TransformComponent]
* - Definition : Transform 정보를 담는 컴포넌트
* - LifeTime : AddComponent -> Destroy
* - Inheritence : Component
* - Access : SceneObject::GetTransformComponent
* - Responsibility :
*	- Transform : 컴포넌트의 행렬
* - Note : 기본적으로 Local Space, World Space는 World 접두사를 붙인다.
*/
class TransformComponent : public Component
{
public:
	REFLECT_GENERATED_BODY()
	TransformComponent(GameObject* owner) : Component(owner) {}
	~TransformComponent() = default;
	
	// World Space
	DirectX::XMMATRIX GetWorldMatrix() const;
	DirectX::XMMATRIX GetWorldInvMatrix() const					{ return XMMatrixInverse(nullptr, GetWorldMatrix()); }
	DirectX::XMFLOAT3 GetWorldPosition() const
	{
		XMFLOAT3 pos;
		XMStoreFloat3(&pos, GetWorldMatrix().r[3]);
		return pos;
	}
	void SetWorldPosition(const DirectX::XMFLOAT3& worldPos);
	DirectX::XMVECTOR GetWorldRotationQuat() const;
	void SetWorldRotation(const DirectX::XMFLOAT3& worldRot);
	void SetWorldRotation(const DirectX::XMVECTOR& worldQuat);

	// Local Space
	DirectX::XMMATRIX GetLocalMatrix() const;
	DirectX::XMVECTOR GetRightVec() const						{ return DirectX::XMVector3Normalize(GetWorldMatrix().r[0]); }
	DirectX::XMVECTOR GetUpVec() const							{ return DirectX::XMVector3Normalize(GetWorldMatrix().r[1]); }
	DirectX::XMVECTOR GetForwardVec() const						{ return DirectX::XMVector3Normalize(GetWorldMatrix().r[2]); }
	DirectX::XMFLOAT3 GetPosition() const						{ return m_transform.position; }
	void SetPosition(const DirectX::XMFLOAT3& pos)				{ m_transform.position = pos; }
	DirectX::XMFLOAT3 GetRotation() const						{ return m_transform.rotation; }
	void SetRotation(const DirectX::XMFLOAT3& rotation)			{ m_transform.rotation = rotation; }
	void SetRotation(const DirectX::XMVECTOR& quat)				{ m_transform.rotation = ToEulerFromQuat(quat); }
	DirectX::XMFLOAT3 GetScale() const							{ return m_transform.scale; }
	void SetScale(const DirectX::XMFLOAT3& scale)				{ m_transform.scale = scale; }

	DirectX::XMFLOAT3 GetRight() const
	{
		XMFLOAT3 pos;
		XMStoreFloat3(&pos, DirectX::XMVector3Normalize(GetWorldMatrix().r[0]));
		return pos;
	}
	DirectX::XMFLOAT3 GetUp() const
	{
		XMFLOAT3 pos;
		XMStoreFloat3(&pos, DirectX::XMVector3Normalize(GetWorldMatrix().r[1]));
		return pos;
	}
	
	DirectX::XMFLOAT3 GetForward() const
	{
		XMFLOAT3 pos;
		XMStoreFloat3(&pos, DirectX::XMVector3Normalize(GetWorldMatrix().r[2]));
		return pos;
	}

	void SetTransform(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot, const DirectX::XMFLOAT3& scale) 
	{
		m_transform.position = pos;
		m_transform.rotation = rot;
		m_transform.scale = scale;
	}

	void Move(const DirectX::XMFLOAT3& delta);
	void Rotate(const DirectX::XMVECTOR& deltaQuat);
	void Scale(const DirectX::XMFLOAT4& scaleFactor);
	void LookTo(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& up = { 0.0f, 1.0f, 0.0f });
	void LookAt(const DirectX::XMFLOAT3& targetPos, const DirectX::XMFLOAT3& up = { 0.0f, 1.0f, 0.0f });

private:
	Transform m_transform;
};

