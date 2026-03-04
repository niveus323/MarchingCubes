#include "pch.h"
#include "LightComponent.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Scene/Component/BillboardComponent.h"

BEGIN_ENUM_REFLECTION(ELightType)
    REFLECT_ENUM(ELightType::Directional)
    REFLECT_ENUM(ELightType::Point)
    REFLECT_ENUM(ELightType::Spot)
END_ENUM_REFLECTION(ELightType)

BEGIN_REFLECTION(LightComponent, TransformableComponent)
    REFLECT_PROPERTY_ENUM(m_type, ELightType, "Type")
    REFLECT_PROPERTY(m_radiance, EPropertyType::Vector3, "Radiance")
    // Range : Point, Spot 일때만
    REFLECT_PROPERTY_EXPR_IF(m_range, EPropertyType::Float, "Range", inst->m_type != ELightType::Directional)
    // SpotInnerCos : Spot일 때만
    REFLECT_PROPERTY_EXPR_IF(m_spotInnerCos, EPropertyType::Float, "InnerCos", inst->m_type == ELightType::Spot)
END_REFLECTION()

void LightComponent::Init()
{
    TransformableComponent::Init();
    if (auto scene = GetScene()) scene->RegisterLight(this);

    auto billboard = GetOwner()->GetComponent<BillboardComponent>();
    if (!billboard) billboard = GetOwner()->AddComponent<BillboardComponent>(EObjectFlags::Transient | EObjectFlags::EditorOnly);

    auto lightIcon = EngineCore::GetResourceManager()->LoadTextureAsset("Icons/Light_64.png");
    billboard->SetIcon(lightIcon, 2);
}

void LightComponent::Destroy()
{
    if (auto scene = GetScene()) scene->UnregisterLight(this);
}

void LightComponent::Serialize(Serializer& ar)
{
    Component::Serialize(ar);
    int typeValue = static_cast<int>(m_type);
    ar.Serialize("LightType", typeValue);
    if (!ar.IsSaving()) m_type = static_cast<ELightType>(typeValue);
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

    switch (m_type)
    {
        case ELightType::Directional:
        {
            data.dirOrPos = m_transformCache->GetForward();
        }
        break;
        case ELightType::Spot:
        case ELightType::Point:
        default:
        {
            data.dirOrPos = m_transformCache->GetWorldPosition();
        }
        break;
    }

    return data;
}

DirectX::XMFLOAT3 LightComponent::GetLightDirection() const
{
    return m_transformCache->GetForward();
}

void LightComponent::SetLightDirection(const DirectX::XMFLOAT3& newDir)
{
    m_transformCache->LookTo(newDir);
}
