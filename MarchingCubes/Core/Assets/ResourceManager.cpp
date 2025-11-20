#include "pch.h"
#include "ResourceManager.h"
#include "TextureRegistry.h"
#include "MaterialRegistry.h"

ResourceManager::ResourceManager(ID3D12Device* device, UploadContext* uploadContext, DescriptorAllocator* descriptorAllocator) :
	m_device(device),
	m_uploadContext(uploadContext),
	m_descriptorAllocator(descriptorAllocator)
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	m_textureRegistry = std::make_unique<TextureRegistry>(TextureRegistry::InitInfo{
		.device = m_device,
		.upload = m_uploadContext,
		.descriptorAllocator = m_descriptorAllocator
	});
	m_materialRegistry = std::make_unique<MaterialRegistry>(MaterialRegistry::InitInfo{
		.device = m_device,
		.upload = m_uploadContext,
		.descriptorAllocator = m_descriptorAllocator,
		.textureRegistry = m_textureRegistry.get()
	});
}

ResourceManager::~ResourceManager()
{
	CoUninitialize();
}

void ResourceManager::syncGpu(ID3D12GraphicsCommandList* cmd)
{
	m_textureRegistry->syncGpu(cmd);
}

void ResourceManager::BindDescriptorTable(ID3D12GraphicsCommandList* cmd)
{
	m_materialRegistry->BindDescriptorTable(cmd);
	m_textureRegistry->BindDescriptorTable(cmd);
}

void ResourceManager::BuildTables(ID3D12GraphicsCommandList* cmd)
{
	m_materialRegistry->BuildTable(cmd);
}

// 텍스쳐 로드 (엔진이 켜져 있는 동안 동일한 경로의 리소스는 바뀌지 않는다고 가정)
uint32_t ResourceManager::LoadTexture(const std::wstring& path)
{
	return m_textureRegistry->LoadTexture(path);
}

size_t ResourceManager::AddMaterial(const MaterialCPU& material)
{
	return m_materialRegistry->AddMaterial(material);
}
