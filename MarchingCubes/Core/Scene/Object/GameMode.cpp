#include "pch.h"
#include "GameMode.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/PlayerController.h"
#include "Core/Scene/Object/Pawn.h"

void GameMode::Init()
{
    if (Scene* scene = GetScene())
    {
        m_pc = scene->CreateObject<PlayerController>();
        Pawn* defaultPawn = scene->CreateObject<Pawn>();
        defaultPawn->SetPosition({ 0.0f, 2.0f, 0.0f });
        m_pc->Possess(defaultPawn);
    }
}
