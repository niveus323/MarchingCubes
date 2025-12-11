#include "pch.h"
#include "TransformComponent.h"
#include "Core/Scene/SceneObject.h"

DirectX::XMMATRIX TransformComponent::GetWorldMatrix() const
{
	const auto& position = m_transform.position;
	const auto& rotation = m_transform.rotation;
	const auto& scale = m_transform.scale;

	XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
	XMMATRIX R = XMMatrixRotationQuaternion(ToQuatFromEuler(rotation));
	XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);

	XMMATRIX local = S * R * T;
	// °èÃþÇü Transform Àû¿ë
	if (auto parent = m_owner->GetOwner())
	{
		if (auto P = parent->GetTransformComponent())
		{
			return local * P->GetWorldMatrix();
		}
	}

	return local;
}

DirectX::XMMATRIX TransformComponent::GetWorldInvMatrix() const
{
	return XMMatrixInverse(nullptr, GetWorldMatrix());
}

void TransformComponent::Move(const DirectX::XMFLOAT3& delta)
{
	m_transform.position.x += delta.x;
	m_transform.position.y += delta.y;
	m_transform.position.z += delta.z;
}

void TransformComponent::Rotate(const DirectX::XMVECTOR& deltaQuat)
{
	XMVECTOR current = ToQuatFromEuler(m_transform.rotation);
	XMVECTOR result = XMQuaternionNormalize(XMQuaternionMultiply(current, deltaQuat));
	m_transform.rotation = ToEulerFromQuat(result);
}

void TransformComponent::Scale(const DirectX::XMFLOAT4& scaleFactor)
{
	m_transform.scale.x *= scaleFactor.x;
	m_transform.scale.y *= scaleFactor.y;
	m_transform.scale.z *= scaleFactor.z;
}
