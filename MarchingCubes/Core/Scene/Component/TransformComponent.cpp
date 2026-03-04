#include "pch.h"
#include "TransformComponent.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Math/MathHelper.h"

BEGIN_REFLECTION(TransformComponent, Component)
	REFLECT_PROPERTY(m_transform.position, EPropertyType::Vector3, "Position")
	REFLECT_PROPERTY(m_transform.rotation, EPropertyType::Vector3, "Rotation")
	REFLECT_PROPERTY(m_transform.scale, EPropertyType::Vector3, "Scale")
END_REFLECTION()

void TransformComponent::Serialize(Serializer& ar)
{
	Component::Serialize(ar);
	ar.Serialize("Transform", m_transform);
}

DirectX::XMMATRIX TransformComponent::GetWorldMatrix() const
{
	XMMATRIX localMat = GetLocalMatrix();

	if (auto parent = GetOwner()->GetOwner())
	{
		if (auto parentTransform = parent->GetComponent<TransformComponent>())
		{
			return localMat * parentTransform->GetWorldMatrix();
		}
	}

	return localMat;
}

void TransformComponent::SetWorldPosition(const DirectX::XMFLOAT3& worldPos)
{
	auto parent = GetOwner()->GetOwner();
	if (!parent)
	{
		SetPosition(worldPos);
		return;
	}

	if (auto parentTransform = parent->GetComponent<TransformComponent>())
	{
		XMMATRIX parentWorld = parentTransform->GetWorldMatrix();
		XMMATRIX parentInv = XMMatrixInverse(nullptr, parentWorld);

		XMVECTOR vWorld = XMLoadFloat3(&worldPos);
		XMVECTOR vLocal = XMVector3TransformCoord(vWorld, parentInv);

		XMStoreFloat3(&m_transform.position, vLocal);
	}
}

DirectX::XMVECTOR TransformComponent::GetWorldRotationQuat() const
{
	XMVECTOR localRot = ToQuatFromEuler(m_transform.rotation);

	if (auto parent = GetOwner()->GetOwner())
	{
		if (auto parentTransform = parent->GetComponent<TransformComponent>())
		{
			XMVECTOR parentWorldRot = parentTransform->GetWorldRotationQuat();
			return XMQuaternionMultiply(localRot, parentWorldRot);
		}
	}

	return localRot;
}

DirectX::XMFLOAT3 TransformComponent::GetWorldRotation() const
{
	return ToEulerFromQuat(GetWorldRotationQuat());
}

void TransformComponent::SetWorldRotation(const DirectX::XMFLOAT3& worldRotEuler)
{
    XMVECTOR worldQuat = ToQuatFromEuler(worldRotEuler);
    SetWorldRotation(worldQuat);
}

void TransformComponent::SetWorldRotation(const DirectX::XMVECTOR& worldRotQuat)
{
	XMVECTOR localQuat = worldRotQuat;
	if (auto parent = GetOwner()->GetOwner())
	{
		if (auto parentTransform = parent->GetComponent<TransformComponent>())
		{
			XMVECTOR parentWorldRot = parentTransform->GetWorldRotationQuat();
			XMVECTOR parentRotInv = XMQuaternionInverse(parentWorldRot);
			localQuat = XMQuaternionMultiply(worldRotQuat, parentRotInv);
		}
	}
	SetRotation(localQuat);
}

DirectX::XMFLOAT3 TransformComponent::GetWorldScale() const
{
	DirectX::XMVECTOR scale, rotQuat, trans;
	DirectX::XMMATRIX worldMat = GetWorldMatrix();

	if (DirectX::XMMatrixDecompose(&scale, &rotQuat, &trans, worldMat))
	{
		DirectX::XMFLOAT3 worldScale;
		DirectX::XMStoreFloat3(&worldScale, scale);
		return worldScale;
	}

	return { 1.0f, 1.0f, 1.0f };
}

void TransformComponent::SetWorldScale(const DirectX::XMFLOAT3& worldScale)
{
	auto parent = GetOwner()->GetOwner();
	if (!parent)
	{
		SetScale(worldScale);
		return;
	}

	if (auto parentTransform = parent->GetComponent<TransformComponent>())
	{
		DirectX::XMFLOAT3 parentScale = parentTransform->GetWorldScale();
		DirectX::XMFLOAT3 newLocalScale;
		newLocalScale.x = (std::abs(parentScale.x) > MathHelper::Epsilon) ? (worldScale.x / parentScale.x) : 0.0f;
		newLocalScale.y = (std::abs(parentScale.y) > MathHelper::Epsilon) ? (worldScale.y / parentScale.y) : 0.0f;
		newLocalScale.z = (std::abs(parentScale.z) > MathHelper::Epsilon) ? (worldScale.z / parentScale.z) : 0.0f;
		SetScale(newLocalScale);
	}
}

DirectX::XMMATRIX TransformComponent::GetLocalMatrix() const
{
	XMMATRIX T = XMMatrixTranslation(m_transform.position.x, m_transform.position.y, m_transform.position.z);
	XMMATRIX R = XMMatrixRotationQuaternion(ToQuatFromEuler(m_transform.rotation));
	XMMATRIX S = XMMatrixScaling(m_transform.scale.x, m_transform.scale.y, m_transform.scale.z);

	return S * R * T;
}

void TransformComponent::SetRotation(const DirectX::XMVECTOR& quat)
{
	m_transform.rotation = ToEulerFromQuat(quat);
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
	if (XMVectorGetX(XMVector3LengthSq(vDir)) <= MathHelper::TolerantEpsilon) return;

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
