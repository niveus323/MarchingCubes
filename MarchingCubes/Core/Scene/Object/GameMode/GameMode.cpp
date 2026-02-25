#include "pch.h"
#include "GameMode.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/Controller/PlayerController.h"
#include "Core/Scene/Object/Pawn.h"

BEGIN_REFLECTION(GameMode, GameObject)
END_REFLECTION()

void GameMode::Init()
{
    GameObject::Init();
}

void GameMode::Destroy()
{
    GameObject::Destroy();
}

void GameMode::BeginPlay()
{
    GameObject::BeginPlay();
    if (auto scene = GetScene())
    {
        m_pc = scene->CreateObject<PlayerController>("PlayerController");
        Pawn* defaultPawn = scene->CreateObject<Pawn>("Pawn");
        defaultPawn->SetPosition({ 0.0f, 2.0f, 0.0f });
        m_pc->Possess(defaultPawn);
    }
}

void GameMode::EndPlay()
{
    GameObject::EndPlay();
    m_pc = nullptr;
}
