#pragma once
#include "Component.h"
#include "Core/DataStructures/ShaderTypes.h"

class LightComponent : public Component
{
	REFLECT_GENERATED_BODY()
public:
	LightComponent(GameObject* owner, ELightType type = ELightType::Directional, DirectX::XMFLOAT3 radiance = { 1.0f, 1.0f, 1.0f }, float range = 100.0f, float spotInnerCos = 0.9f);
	virtual ~LightComponent();

	Light GetLightInfo() const;
	DirectX::XMFLOAT3 GetLightDirection() const;
	void SetLightDirection(const DirectX::XMFLOAT3& newDir);

private:
	ELightType m_type = ELightType::Directional;
	DirectX::XMFLOAT3 m_radiance = { 1.0f, 1.0f, 1.0f };
	float m_range = 100.0f;
	float m_spotInnerCos = 0.9f;
};

