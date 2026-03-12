#include "pch.h"
#include "Pawn.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/GameMode/GameMode.h"
#include "Core/Scene/Object/Controller/PlayerController.h"
#include "Core/Scene/Component/CameraComponent.h"

BEGIN_ENUM_REFLECTION(EAutoPossessTarget)
    REFLECT_ENUM(EAutoPossessTarget::Disabled)
    REFLECT_ENUM(EAutoPossessTarget::Player0)
    REFLECT_ENUM(EAutoPossessTarget::Player1)
    REFLECT_ENUM(EAutoPossessTarget::Player2)
    REFLECT_ENUM(EAutoPossessTarget::Player3)
END_ENUM_REFLECTION(EAutoPossessTarget)

BEGIN_REFLECTION(Pawn, SceneObject)
    REFLECT_PROPERTY_ENUM(m_autoPossess, EAutoPossessTarget, "AutoPossess")
END_REFLECTION()

void Pawn::BeginPlay()
{
    SceneObject::BeginPlay();
    if (m_autoPossess != EAutoPossessTarget::Disabled)
    {
        auto scene = GetScene();
        assert(scene);
        if (GameMode* gm = scene->GetGameMode())
        {
            if (Controller* pc = gm->GetController(static_cast<int>(m_autoPossess)))
            {
                pc->Possess(this);
            }
        }
    }
}

void Pawn::OnPossess()
{
    CameraComponent* targetCamera = GetComponent<CameraComponent>();
    // 카메라가 없으면 자체적으로 카메라 생성
    if (!targetCamera)
    {
        targetCamera = AddComponent<CameraComponent>(EObjectFlags::Transient | EObjectFlags::EditorOnly);
        Log::Print("Pawn", "카메라가 없어 임시 CameraComponent를 생성했습니다.");
    }
}

void Pawn::AddMovementInput(DirectX::XMVECTOR dir, float scale)
{
    if (auto transformComp = GetTransformComponent())
    {
        DirectX::XMVECTOR currentPos = DirectX::XMLoadFloat3(&transformComp->GetPosition());
        DirectX::XMVECTOR offset = DirectX::XMVectorScale(dir, scale);

        DirectX::XMFLOAT3 newPos;
        DirectX::XMStoreFloat3(&newPos, DirectX::XMVectorAdd(currentPos, offset));

        transformComp->SetPosition(newPos);
    }
}

void Pawn::AddControllerYawInput(float val)
{
    if (auto transformComp = GetTransformComponent())
    {
        DirectX::XMFLOAT3 rot = transformComp->GetRotation();
        rot.y += val;
        transformComp->SetRotation(rot);
    }
}

void Pawn::AddControllerPitchInput(float val)
{
    if (auto transformComp = GetTransformComponent())
    {
        DirectX::XMFLOAT3 rot = transformComp->GetRotation();
        rot.x = std::clamp(rot.x + val, -DirectX::XM_PIDIV2 + 0.01f, DirectX::XM_PIDIV2 - 0.01f);
        transformComp->SetRotation(rot);
    }
}
