#include "pch.h"
#include "CameraComponent.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Scene/Component/BillboardComponent.h"

BEGIN_REFLECTION(CameraComponent, TransformableComponent)
	REFLECT_PROPERTY(m_fov, EPropertyType::Float, "Fov")
	REFLECT_PROPERTY(m_nearZ, EPropertyType::Float, "NearZ")
	REFLECT_PROPERTY(m_farZ, EPropertyType::Float, "FarZ")
END_REFLECTION()

void CameraComponent::Init()
{
	TransformableComponent::Init();
	auto billboard = GetOwner()->GetComponent<BillboardComponent>();
	if (!billboard) billboard = GetOwner()->AddComponent<BillboardComponent>(EObjectFlags::Transient | EObjectFlags::EditorOnly);

	auto icon = EngineCore::GetResourceManager()->LoadTextureAsset("Icons/Camera_64.png");
	billboard->SetIcon(icon, 1);
}

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
	auto* transform = GetTransformComp();
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