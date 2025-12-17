#include "pch.h"
#include "PlayerController.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Input/InputState.h"

void PlayerController::Update(float deltaTime)
{
    GameObject::Update(deltaTime);
    if (m_possessed)
    {
        ProcessInput(deltaTime);
    }
}

void PlayerController::Possess(Pawn* pawn)
{
    m_possessed = pawn;
    m_possessed->OnPossess();
}

void PlayerController::UnPossess()
{
    m_possessed->OnUnPossess();
    m_possessed = nullptr;
}

void PlayerController::ProcessInput(float deltaTime)
{
    auto input = EngineCore::GetInputState();

    if (input->m_rightBtnState == ActionKeyState::Pressed)
    {
        float deltaX = input->m_mouseDeltaX;
        float deltaY = -input->m_mouseDeltaY;

        m_possessed->AddControllerYawInput(deltaX * m_mouseSensitivity);
        m_possessed->AddControllerPitchInput(deltaY * m_mouseSensitivity);
    }

    XMVECTOR moveDir = DirectX::XMVectorZero();
    XMVECTOR forward = m_possessed->GetForwardVector();
    XMVECTOR right = m_possessed->GetRightVector();
    XMVECTOR up = m_possessed->GetUpVector();

    if (input->IsPressed(ActionKey::MoveForward))  moveDir = XMVectorAdd(moveDir, forward);
    if (input->IsPressed(ActionKey::MoveBackward)) moveDir = XMVectorSubtract(moveDir, forward);
    if (input->IsPressed(ActionKey::MoveRight))    moveDir = XMVectorAdd(moveDir, right);
    if (input->IsPressed(ActionKey::MoveLeft))     moveDir = XMVectorSubtract(moveDir, right);
    if (input->IsPressed(ActionKey::MoveUp))       moveDir = XMVectorAdd(moveDir, up);
    if (input->IsPressed(ActionKey::MoveDown))     moveDir = XMVectorSubtract(moveDir, up);

    if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f)
    {
        moveDir = XMVector3Normalize(moveDir);
        m_possessed->AddMovementInput(moveDir, m_moveSpeed * deltaTime);
    }
}
