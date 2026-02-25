#pragma once
#include "Component.h"
#include "Core/DataStructures/ShaderTypes.h"

class LightComponent : public Component
{
	REFLECT_GENERATED_BODY(LightComponent)
public:
	virtual void Init() override;
	virtual void Destroy() override;
	void Serialize(Serializer& ar);

	Light GetLightInfo() const;
	DirectX::XMFLOAT3 GetLightDirection() const;
	void SetLightDirection(const DirectX::XMFLOAT3& newDir);

private:
	ELightType m_type = ELightType::Directional;
	DirectX::XMFLOAT3 m_radiance = { 1.0f, 1.0f, 1.0f };
	float m_range = 100.0f;
	float m_spotInnerCos = 0.9f;
};

