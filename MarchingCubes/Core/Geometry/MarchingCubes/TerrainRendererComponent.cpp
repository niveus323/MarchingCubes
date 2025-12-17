#include "pch.h"
#include "TerrainRendererComponent.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Scene.h"
#include "Core/Engine/EngineCore.h"

void TerrainRendererComponent::Submit()
{
    if (!m_chunkRenderer) return;

    if (auto renderSystem = EngineCore::GetRenderSystem())
    {
        m_chunkRenderer->Submit(renderSystem, GetOwner<SceneObject>()->GetWorldTransform(), m_materialInstance);
    }

}
