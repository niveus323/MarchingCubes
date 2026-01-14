#include "pch.h"
#include "CameraComponent.h"
#include "Core/Scene/Object/SceneObject.h"

BEGIN_REFLECTION(CameraComponent, Component)
	REFLECT_PROPERTY(m_fov, EPropertyType::Float)
	REFLECT_PROPERTY(m_nearZ, EPropertyType::Float)
	REFLECT_PROPERTY(m_farZ, EPropertyType::Float)
END_REFLECTION()

CameraConstants CameraComponent::GetCameraConstants() const
{
	CameraConstants cb{};
	XMMATRIX vp = GetViewProjMatrix();
	XMStoreFloat4x4(&cb.viewProjMatrix, XMMatrixTranspose(vp));
	
	cb.cameraPosition = GetOwner<SceneObject>()->GetWorldPosition();
	return cb;
}

XMMATRIX CameraComponent::GetViewMatrix() const
{
	auto* transform = GetOwner<SceneObject>()->GetTransformComponent();
	if(!transform) return XMMatrixIdentity();

	XMFLOAT3 pos = transform->GetWorldPosition();
	XMVECTOR posVec = XMLoadFloat3(&pos);
	return XMMatrixLookToLH(posVec, transform->GetForwardVec(), transform->GetUpVec());
}

XMVECTOR CameraComponent::GetForwardVector() const
{
	return GetOwner<SceneObject>()->GetForwardVector();
}

XMVECTOR CameraComponent::GetRightVector() const
{
	return GetOwner<SceneObject>()->GetRightVector();
}

XMVECTOR CameraComponent::GetUpVector() const
{
	return GetOwner<SceneObject>()->GetUpVector();
}