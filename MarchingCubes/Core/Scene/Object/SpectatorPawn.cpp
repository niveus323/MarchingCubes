#include "pch.h"
#include "SpectatorPawn.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Scene.h"

SpectatorPawn::SpectatorPawn(Scene* scene) : 
    Pawn(scene)
{
    auto viewport = scene->GetViewport();
    m_cameraComp = AddComponent<CameraComponent>(viewport.Width, viewport.Height);

    // Default Position
    SetPosition({ 0.0f, 5.0f, -10.0f });
}

void SpectatorPawn::Init()
{
    Pawn::Init();
}
