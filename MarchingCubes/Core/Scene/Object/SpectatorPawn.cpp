#include "pch.h"
#include "SpectatorPawn.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Scene.h"
#include "Core/Rendering/RenderSystem.h"

BEGIN_REFLECTION(SpectatorPawn, Pawn)
END_REFLECTION()

void SpectatorPawn::Init()
{
    Pawn::Init();
    AddFlags(EObjectFlags::EditorOnly);
    auto viewport = EngineCore::GetRenderSystem()->GetViewport();
    m_cameraComp = AddComponent<CameraComponent>(EObjectFlags::EditorOnly);
    m_cameraComp->SetViewport(viewport.Width, viewport.Height);

    // Default Position
    SetPosition({ 0.0f, 5.0f, -10.0f });
}
