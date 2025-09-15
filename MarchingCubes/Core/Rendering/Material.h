#pragma once
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;

class Material
{
public:
	Material() = default;
	~Material();
	void CreateConstantBuffer(ID3D12Device* device);
	void Update(float deltaTime);
	void BindConstant(ID3D12GraphicsCommandList* cmdList) const;

public:
	void SetAlbedo(const DirectX::XMFLOAT3& albedo) { m_cb.albedo = albedo; }
	void SetMetallic(float metallic)				{ m_cb.metallic = metallic; }
	void SetRoughness(float roughness)				{ m_cb.roughness = roughness; }
	void SetSpecularStrength(float spec)			{ m_cb.specularStrength = spec; }
	void SetAmbientOcclusion(float ao)				{ m_cb.ao = ao; }
	void SetIOR(float ior)							{ m_cb.ior = ior; }
	void SetShadingModel(EShadingModel model)		{ m_cb.shadingModel = model; }
	void SetOpacity(float opacity) { m_cb.opacity = opacity; }

private:
	ComPtr<ID3D12Resource>  m_buffer;
	UINT8*					m_mappedData{};
	MaterialConstants		m_cb{};
	
};

