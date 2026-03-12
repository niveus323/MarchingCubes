#include "pch.h"
#include "SpectatorPawn.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(SpectatorPawn, Pawn)
END_REFLECTION()

void SpectatorPawn::Init()
{
    Pawn::Init();
    AddFlags(EObjectFlags::EditorOnly);
    m_cameraComp = AddComponent<CameraComponent>(EObjectFlags::EditorOnly);

    // Default Position
    SetPosition({ 0.0f, 5.0f, -10.0f });
}
