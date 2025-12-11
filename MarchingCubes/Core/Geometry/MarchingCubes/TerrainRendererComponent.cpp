#include "pch.h"
#include "TerrainRendererComponent.h"
#include "Core/Scene/SceneObject.h"
#include "Core/Scene/BaseScene.h"

void TerrainRendererComponent::Submit()
{
    if (!m_chunkRenderer) return;

    RenderSystem* renderSystem = GetScene()->GetRenderSystem();
    if (!renderSystem) return;

    m_chunkRenderer->Submit(renderSystem, m_owner->GetTransform(), m_materialInstance);
}
