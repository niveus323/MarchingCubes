#include "pch.h"
#include "TransformComponent.h"
#include "Core/Scene/Object/SceneObject.h"

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
	while (GameObject* currentAncestor = GetOwner()->GetOwner())
	{
		if (auto parentTransform = currentAncestor->GetComponent<TransformComponent>())
		{
			return local * parentTransform->GetWorldMatrix();
		}

		currentAncestor = currentAncestor->GetOwner();
	}
	return local;
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

void TransformComponent::LookTo(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& up)
{
	XMVECTOR vDir = XMLoadFloat3(&direction);
	XMVECTOR vUp = XMLoadFloat3(&up);
	if (XMVectorGetX(XMVector3LengthSq(vDir)) <= 0.00001f) return;

	vDir = XMVector3Normalize(vDir);

	if (XMVector3NearEqual(XMVectorAbs(vDir), XMVectorAbs(vUp), XMVectorSet(0.001f, 0.001f, 0.001f, 0.0f)))
	{
		vUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	XMMATRIX viewMat = XMMatrixLookToLH(XMVectorZero(), vDir, vUp);
	XMMATRIX worldRotMat = XMMatrixTranspose(viewMat);
	XMVECTOR rotQuat = XMQuaternionRotationMatrix(worldRotMat);
	SetRotation(rotQuat);
}

void TransformComponent::LookAt(const DirectX::XMFLOAT3& targetPos, const DirectX::XMFLOAT3& up)
{
	XMVECTOR vPos = XMLoadFloat3(&m_transform.position);
	XMVECTOR vTarget = XMLoadFloat3(&targetPos);

	XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);

	XMFLOAT3 dir;
	XMStoreFloat3(&dir, vDir);

	LookTo(dir, up);
}
