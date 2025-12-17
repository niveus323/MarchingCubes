#include "pch.h"
#include "CameraComponent.h"
#include "Core/Scene/Object/SceneObject.h"

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