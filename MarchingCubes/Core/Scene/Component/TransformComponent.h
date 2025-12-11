#pragma once
#include "Component.h"
#include "Core/DataStructures/Data.h"

class TransformComponent : public Component
{
public:
	TransformComponent(SceneObject* owner) : Component(owner) {}
	~TransformComponent() = default;
	
	DirectX::XMMATRIX GetWorldMatrix() const;
	DirectX::XMMATRIX GetWorldInvMatrix() const;

	DirectX::XMFLOAT3 GetPosition() const { return m_transform.position; }
	void SetPosition(const DirectX::XMFLOAT3& pos) { m_transform.position = pos; }
	
	DirectX::XMFLOAT3 GetRotation() const { return m_transform.rotation; }
	void SetRotation(const DirectX::XMFLOAT3& rotation) { m_transform.rotation = rotation; }
	void SetRotation(const DirectX::XMVECTOR& quat) { m_transform.rotation = ToEulerFromQuat(quat); }
	
	DirectX::XMFLOAT3 GetScale() const { return m_transform.scale; }
	void SetScale(const DirectX::XMFLOAT3& scale) { m_transform.scale = scale; }

	void SetTransform(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot, const DirectX::XMFLOAT3& scale) 
	{
		m_transform.position = pos;
		m_transform.rotation = rot;
		m_transform.scale = scale;
	}

	void Move(const DirectX::XMFLOAT3& delta);
	void Rotate(const DirectX::XMVECTOR& deltaQuat);
	void Scale(const DirectX::XMFLOAT4& scaleFactor);

private:
	Transform m_transform;
};

