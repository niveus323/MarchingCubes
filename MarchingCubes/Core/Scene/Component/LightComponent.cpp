#include "pch.h"
#include "LightComponent.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_ENUM_REFLECTION(ELightType)
    REFLECT_ENUM(ELightType::Directional)
    REFLECT_ENUM(ELightType::Point)
    REFLECT_ENUM(ELightType::Spot)
END_ENUM_REFLECTION(ELightType)

BEGIN_REFLECTION(LightComponent, Component)
    REFLECT_PROPERTY_ENUM(m_type, ELightType)
    REFLECT_PROPERTY(m_radiance, EPropertyType::Vector3)
    // Range : Point, Spot 일때만
    REFLECT_PROPERTY_EXPR_IF(m_range, EPropertyType::Float, inst->m_type != ELightType::Directional)
    // SpotInnerCos : Spot일 때만
    REFLECT_PROPERTY_EXPR_IF(m_spotInnerCos, EPropertyType::Float, inst->m_type == ELightType::Spot)
END_REFLECTION()

void LightComponent::Init()
{
    Component::Init();
    if (auto scene = GetScene()) scene->RegisterLight(this);
}

void LightComponent::Destroy()
{
    if (auto scene = GetScene()) scene->UnregisterLight(this);
}

void LightComponent::Serialize(Serializer& ar)
{
    Component::Serialize(ar);
    //int typeValue = static_cast<int>(m_type);
    //ar.Serialize("LightType", typeValue);
    ar.Serialize("Radiance", m_radiance);
    ar.Serialize("Range", m_range);
    ar.Serialize("SpotInnerCos", m_spotInnerCos);
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

DirectX::XMFLOAT3 LightComponent::GetLightDirection() const
{
    if (auto transform = GetOwner()->GetComponent<TransformComponent>())
        return transform->GetForward();
    return { 0.f, 0.f, 1.f };
}

void LightComponent::SetLightDirection(const DirectX::XMFLOAT3& newDir)
{
    if (auto transform = GetOwner()->GetComponent<TransformComponent>())
        transform->LookTo(newDir);
}
