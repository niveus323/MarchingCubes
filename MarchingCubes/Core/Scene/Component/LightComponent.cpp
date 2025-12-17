#include "pch.h"
#include "LightComponent.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

LightComponent::LightComponent(GameObject* owner, ELightType type, DirectX::XMFLOAT3 radiance, float range, float spotInnerCos) :
    Component(owner),
    m_type(type),
    m_radiance(radiance),
    m_range(range),
    m_spotInnerCos(0.9f)
{
    if (auto scene = GetScene()) scene->RegisterLight(this);
}

LightComponent::~LightComponent()
{
    if (auto scene = GetScene()) scene->UnregisterLight(this);
}

Light LightComponent::GetLightInfo() const
{
    Light data = {
        .type = m_type,
        .radiance = m_radiance,
        .rangeOrPadding = m_range,
        .spotInnerCos = m_spotInnerCos
    };

    if (auto transform = GetOwner()->GetComponent<TransformComponent>())
    {
        switch (m_type)
        {
            case ELightType::Directional:
            {
                data.dirOrPos = transform->GetForward();
            }
            break;
            case ELightType::Spot:
            case ELightType::Point:
            default:
            {
                data.dirOrPos = transform->GetWorldPosition();
            }
            break;
        }
    }

    return data;
}
