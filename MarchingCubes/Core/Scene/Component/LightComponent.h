#pragma once
#include "Component.h"
#include "Core/DataStructures/ShaderTypes.h"

class LightComponent : public Component
{
public:
	LightComponent(GameObject* owner, ELightType type = ELightType::Directional, DirectX::XMFLOAT3 radiance = { 1.0f, 1.0f, 1.0f }, float range = 100.0f, float spotInnerCos = 0.9f);
	virtual ~LightComponent();

	Light GetLightInfo() const;
private:
	ELightType m_type = ELightType::Directional;
	DirectX::XMFLOAT3 m_radiance = { 1.0f, 1.0f, 1.0f };
	float m_range = 100.0f;
	float m_spotInnerCos = 0.9f;
};

