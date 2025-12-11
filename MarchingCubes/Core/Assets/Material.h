#pragma once
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;
using MaterialHandle = uint32_t;

class Material
{
public:
	Material() {}
	~Material() = default;

public:
	void SetMaterialConstants(const MaterialConstants& cb)			{ m_cb = cb; }
	void SetAlbedo(const DirectX::XMFLOAT3& albedo)					{ m_cb.albedo = albedo; }
	void SetMetallic(float metallic)								{ m_cb.metallic = metallic; }
	void SetRoughness(float roughness)								{ m_cb.roughness = roughness; }
	void SetSpecularStrength(float spec)							{ m_cb.specularStrength = spec; }
	void SetAmbientOcclusion(float ao)								{ m_cb.ao = ao; }
	void SetIOR(float ior)											{ m_cb.ior = ior; }
	void SetShadingModel(EShadingModel model)						{ m_cb.shadingModel = model; }
	void SetOpacity(float opacity)									{ m_cb.opacity = opacity; }
	void SetTextureParams(const TextureParams& texParams)			{ m_cb.baseTextures = texParams; }
	void SetTextureMapping(const ETextureMappingTypes type)			{ m_cb.baseTextures.mappingType = type; }
	void SetDiffuseTex(const uint32_t diffuseTexHandle)				{ m_diffuseHandle = diffuseTexHandle; }
	void SetNormalTex(const uint32_t normalTexHandle)				{ m_normalHandle = normalTexHandle; }
	void SetArmTex(const uint32_t armTexHandle)						{ m_armHandle = armTexHandle; }
	void SetDisplacementTex(const uint32_t dispTexHandle)			{ m_displaceHandle = dispTexHandle; }
	void SetRoughTex(const uint32_t roughTexHandle)					{ m_roughHandle = roughTexHandle; }
	void SetEmissiveTex(const uint32_t emissiveHandle)				{ m_emissiveHandle = emissiveHandle; }
	void SetMetallicTex(const uint32_t metalicHandle)				{ m_metailicHandle = metalicHandle; }
	void SetTriplanarParams(const TriplanarParams& triplanarParams) { m_cb.baseTextures.triplanar = triplanarParams; }

	MaterialConstants GetConstants() const { return m_cb; }
	const TextureParams& GetTextureParams() const { return m_cb.baseTextures; }
	uint32_t GetDiffuseHandle() const { return m_diffuseHandle; }
	uint32_t GetNormalHandle() const { return m_normalHandle; }
	uint32_t GetARMHandle() const { return m_armHandle; }
	uint32_t GetDisplacementHandle() const { return m_displaceHandle; }
	uint32_t GetRoughHandle() const { return m_roughHandle; }
	uint32_t GetEmissiveHandle() const { return m_emissiveHandle; }
	
private:
	MaterialConstants		m_cb{};
	uint32_t m_diffuseHandle = UINT32_MAX;
	uint32_t m_normalHandle = UINT32_MAX;
	uint32_t m_armHandle = UINT32_MAX;
	uint32_t m_displaceHandle = UINT32_MAX;
	uint32_t m_roughHandle = UINT32_MAX;
	uint32_t m_emissiveHandle = UINT32_MAX;
	uint32_t m_metailicHandle = UINT32_MAX;
};

