#include "pch.h"
#include "Controller.h"
#include "Core/Scene/Object/Pawn.h"

BEGIN_REFLECTION(Controller, GameObject)
END_REFLECTION()

void Controller::Update(float deltaTime)
{
    GameObject::Update(deltaTime);

    if (m_possessed)
    {
        ProcessInput(deltaTime);
    }
}

void Controller::Possess(Pawn* pawn)
{
    m_possessed = pawn;
    m_possessed->OnPossess();
}

void Controller::UnPossess()
{
    m_possessed->OnUnPossess();
    m_possessed = nullptr;
}