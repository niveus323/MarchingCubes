#include "pch.h"
#include "Pawn.h"

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
