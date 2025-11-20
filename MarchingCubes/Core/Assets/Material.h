#pragma once
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;
using MaterialHandle = uint32_t;

class MaterialCPU
{
public:
	MaterialCPU(MaterialConstants constants, uint32_t diffuseTexHandle) : 
		m_cb(constants), m_diffuseHandle(diffuseTexHandle)
	{}
	~MaterialCPU() = default;

public:
	void SetAlbedo(const DirectX::XMFLOAT3& albedo) { m_cb.albedo = albedo; }
	void SetMetallic(float metallic)				{ m_cb.metallic = metallic; }
	void SetRoughness(float roughness)				{ m_cb.roughness = roughness; }
	void SetSpecularStrength(float spec)			{ m_cb.specularStrength = spec; }
	void SetAmbientOcclusion(float ao)				{ m_cb.ao = ao; }
	void SetIOR(float ior)							{ m_cb.ior = ior; }
	void SetShadingModel(EShadingModel model)		{ m_cb.shadingModel = model; }
	void SetOpacity(float opacity) { m_cb.opacity = opacity; }
	void SetDiffuseTex(const TextureParams& diffuse) { m_cb.diffuse = diffuse; }

	MaterialConstants GetConstants() const { return m_cb; }
	const TextureParams& GetDiffuseTexParams() const { return m_cb.diffuse; }
	uint32_t GetDiffuseHandle() const { return m_diffuseHandle; }
private:
	MaterialConstants		m_cb{};
	uint32_t m_diffuseHandle;
};

