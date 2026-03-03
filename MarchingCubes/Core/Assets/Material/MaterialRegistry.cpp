#include "pch.h"
#include "MaterialRegistry.h"
#include "Core/Assets/TextureRegistry.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Assets/TextureAsset.h"
#include "MaterialAsset.h"
#include <ranges>

MaterialRegistry::MaterialRegistry(TextureRegistry* textureRegistry, uint32_t rootSlot) :
	m_textureRegistry(textureRegistry),
	m_rootSlot(rootSlot)
{
	auto descriptorAllocator = EngineCore::GetDescriptorAllocator();
	assert(descriptorAllocator);

	m_descriptorSlot = descriptorAllocator->AllocateStaticSlot();
}
MaterialRegistry::~MaterialRegistry() = default;

void MaterialRegistry::SyncGpu(ID3D12GraphicsCommandList* cmd)
{
	if (m_materials.empty() || !m_bDirty) return; 

	ID3D12Device* device = EngineCore::GetDevice();
	UploadContext* uploadContext = EngineCore::GetUploadContext();
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();
	assert(device && uploadContext && descriptorAllocator);

	std::vector<MaterialConstants> constants;
	constants.reserve(m_materials.size());

	for (const auto& src : m_materials)
	{
		constants.push_back(src.GetConstants());
	}

	if (m_materialBuffer != nullptr)
	{
		BufferHandle oldBuf{
			.res = m_materialBuffer.Detach()
		};
		uploadContext->FreeBufferHandle(oldBuf);
	}

	const UINT64 byteSize = static_cast<UINT64>(constants.size()) * sizeof(MaterialConstants);
	D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_materialBuffer)));
	NAME_D3D12_OBJECT_ALIAS(m_materialBuffer, L"MaterialBuffer");
	uploadContext->UploadStructuredBuffer(cmd, constants.data(), byteSize, m_materialBuffer.Get(), 0);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_materialBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmd->ResourceBarrier(1, &barrier);
	DescriptorAllocator::CreateSRV_Structured(device, m_materialBuffer.Get(), static_cast<uint32_t>(sizeof(MaterialConstants)), descriptorAllocator->GetStaticCpu(m_descriptorSlot));

	m_bDirty = false;
}

void MaterialRegistry::BindDescriptorTable(ID3D12GraphicsCommandList* cmd) const
{
	if (!m_materialBuffer) return;

	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();
	cmd->SetGraphicsRootDescriptorTable(m_rootSlot, descriptorAllocator->GetStaticGpu(m_descriptorSlot));
}

RegistryIndex MaterialRegistry::RegisterMaterialInstance(const MaterialInstance& matInstance, const std::string& instanceKey)
{
	const auto& baseAsset = matInstance.m_material;
	if (!baseAsset) return UINT32_MAX;

	Material newMat;
	newMat.m_cb = baseAsset->GetConstants();
	auto& matTextures = newMat.m_cb.baseTextures;
	matTextures.diffuseIndex	  = ResolveTextureAsset(baseAsset->GetDiffuse());
	matTextures.normalIndex		  = ResolveTextureAsset(baseAsset->GetNormal());
	matTextures.armIndex		  = ResolveTextureAsset(baseAsset->GetARM());
	matTextures.displacementIndex = ResolveTextureAsset(baseAsset->GetDisplacement());
	matTextures.roughnessIndex	  = ResolveTextureAsset(baseAsset->GetRoughness());
	matTextures.emissiveIndex	  = ResolveTextureAsset(baseAsset->GetEmissive());
	matTextures.metallicIndex	  = ResolveTextureAsset(baseAsset->GetMetallic());

	// NOTE : Material Editor 개발 시 Reflection 기반으로 수정할 것
	for (const auto& [name, value] : matInstance.m_overrides)
	{
		if (name == "DiffuseTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.diffuseIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "NormalTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.normalIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "ARMTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.armIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "DisplacementTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.displacementIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "RoughnessTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.roughnessIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "EmissiveTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.emissiveIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "MetallicTexture")
		{
			if (const auto* texPtr = std::get_if<std::shared_ptr<TextureAsset>>(&value))
				matTextures.metallicIndex = ResolveTextureAsset(*texPtr);
		}
		else if (name == "Albedo")
		{
			if (const XMFLOAT3* albedo = std::get_if<XMFLOAT3>(&value))
				newMat.m_cb.albedo = *albedo;
		}
		else if (name == "Metallic")
		{
			if (const float* f = std::get_if<float>(&value))
				newMat.m_cb.metallic = *f;
		}
		else if (name == "Specular")
		{
			if (const float* f = std::get_if<float>(&value))
				newMat.m_cb.specularStrength = *f;
		}
		else if (name == "Roughness")
		{
			if (const float* roughness = std::get_if<float>(&value)) 
				newMat.m_cb.roughness = *roughness;
		}
		else if (name == "AO")
		{
			if (const float* ao = std::get_if<float>(&value))
				newMat.m_cb.ao = *ao;
		}
		else if (name == "Opacity")
		{
			if (const float* opacity = std::get_if<float>(&value)) 
				newMat.m_cb.opacity = *opacity;
		}
	}

	auto it = m_pathCache.find(instanceKey);
	if (it != m_pathCache.end())
	{
		RegistryIndex existingIndex = it->second;
		m_materials[existingIndex] = newMat;
		m_bDirty = true;
		return existingIndex;
	}

	RegistryIndex resultIndex = static_cast<RegistryIndex>(m_materials.size());
	m_pathCache.insert_or_assign(instanceKey, resultIndex);
	m_materials.push_back(newMat);
	m_bDirty = true;
	return resultIndex;
}

RegistryIndex MaterialRegistry::RegisterMaterialAsset(const std::shared_ptr<MaterialAsset> matAsset)
{
	std::string key = std::filesystem::path(matAsset->GetPath()).string();
	if (m_pathCache.contains(key))
	{
		return m_pathCache[key];
	}
	
	Material newMat;
	newMat.m_cb = matAsset->GetConstants();
	auto& matTextures = newMat.m_cb.baseTextures;
	matTextures.diffuseIndex	  = ResolveTextureAsset(matAsset->GetDiffuse());
	matTextures.normalIndex		  = ResolveTextureAsset(matAsset->GetNormal());
	matTextures.armIndex		  = ResolveTextureAsset(matAsset->GetARM());
	matTextures.displacementIndex = ResolveTextureAsset(matAsset->GetDisplacement());
	matTextures.roughnessIndex	  = ResolveTextureAsset(matAsset->GetRoughness());
	matTextures.emissiveIndex	  = ResolveTextureAsset(matAsset->GetEmissive());
	matTextures.metallicIndex	  = ResolveTextureAsset(matAsset->GetMetallic());
	
	RegistryIndex matHandle = static_cast<RegistryIndex>(m_materials.size());
	m_pathCache.insert_or_assign(key, matHandle);
	m_materials.push_back(newMat);
	m_bDirty = true;
	return matHandle;
}

RegistryIndex MaterialRegistry::FindMaterialHandle(std::string_view path)
{
	if (m_pathCache.find(path.data()) == m_pathCache.end())
	{
		Log::Print("MaterialRegistry", "%s not Loaded.", std::string(path).c_str());
		return UINT32_MAX;
	}
	return m_pathCache[path.data()];
}

const std::string MaterialRegistry::GetMaterialPath(const RegistryIndex handle)
{
	auto iter = std::ranges::find_if(m_pathCache, [&handle](const auto& pair) {
		return pair.second == handle;
	});

	if (iter != m_pathCache.end())
	{
		return iter->first;
	}
	return "";
}

// TextureAsset -> GPU TextureArray 인덱스로 변환
GPUArrayIndex MaterialRegistry::ResolveTextureAsset(const std::shared_ptr<TextureAsset>& asset) const
{
	if (!asset) return UINT32_MAX;
	RegistryIndex textureRegistyIndex = m_textureRegistry->LoadTexture(asset);
	if (textureRegistyIndex == UINT32_MAX) return UINT32_MAX;
	return m_textureRegistry->GetTextureGPUIndex(textureRegistyIndex);
}
