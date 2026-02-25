#include "pch.h"
#include "MaterialRegistry.h"
#include "Core/Assets/TextureRegistry.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
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

	std::unordered_map<uint32_t, uint32_t> indexCache;
	auto GetCachedIndex = [&indexCache, &texReg = m_textureRegistry](uint32_t handle) {
		if (handle == UINT32_MAX) return UINT32_MAX;
		auto it = indexCache.find(handle);
		if (it != indexCache.end()) return it->second;

		uint32_t idx = texReg->GetBindlessIndex(handle);
		indexCache.emplace(handle, idx);
		return idx;
	};

	for (const auto& src : m_materials)
	{
		MaterialConstants dst = src.GetConstants();
		dst.baseTextures.diffuseIndex = GetCachedIndex(src.GetDiffuseHandle());
		dst.baseTextures.normalIndex = GetCachedIndex(src.GetNormalHandle());
		dst.baseTextures.armIndex = GetCachedIndex(src.GetARMHandle());
		dst.baseTextures.displacementIndex = GetCachedIndex(src.GetDisplacementHandle());
		dst.baseTextures.roughnessIndex = GetCachedIndex(src.GetRoughHandle());
		dst.baseTextures.emissiveIndex = GetCachedIndex(src.GetEmissiveHandle());

		constants.push_back(dst);
	}

	if (m_materialBuffer != nullptr)
	{
		BufferHandle oldBuf{};
		oldBuf.res = m_materialBuffer.Detach();
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

uint32_t MaterialRegistry::AddMaterial(const Material& data)
{
	m_materials.push_back(data);
	return static_cast<uint32_t>(m_materials.size() - 1);
}

uint32_t MaterialRegistry::GetMaterialHandle(std::string_view path)
{
	if (m_pathCache.find(path.data()) == m_pathCache.end())
	{
		Log::Print("MaterialRegistry", "%s not Loaded.", std::string(path).c_str());
		return 0;
	}
	return m_pathCache[path.data()];
}

uint32_t MaterialRegistry::GetMaterialHandle(const std::shared_ptr<MaterialAsset> matAsset)
{
	std::string key = std::filesystem::path(matAsset->GetPath()).string();
	if (m_pathCache.contains(key))
	{
		return m_pathCache[key];
	}

	auto GetTextureHandle = [&texReg = m_textureRegistry](std::shared_ptr<TextureAsset> asset) -> uint32_t {
		if (!asset) return UINT32_MAX;
		return texReg->LoadTexture(asset);
	};

	Material newMat;
	newMat.m_cb = matAsset->GetConstants();
	newMat.m_diffuseHandle = GetTextureHandle(matAsset->GetDiffuse());
	newMat.m_normalHandle = GetTextureHandle(matAsset->GetNormal());
	newMat.m_armHandle = GetTextureHandle(matAsset->GetARM());
	newMat.m_displaceHandle = GetTextureHandle(matAsset->GetDisplacement());
	newMat.m_roughHandle = GetTextureHandle(matAsset->GetRoughness());
	newMat.m_emissiveHandle = GetTextureHandle(matAsset->GetEmissive());
	newMat.m_metailicHandle = GetTextureHandle(matAsset->GetMetallic());
	
	uint32_t matHandle = static_cast<uint32_t>(m_materials.size());
	m_pathCache.insert_or_assign(key, matHandle);
	m_materials.push_back(newMat);
	m_bDirty = true;
	return matHandle;
}

const std::string MaterialRegistry::GetMaterialPathByHandle(const uint32_t handle)
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


