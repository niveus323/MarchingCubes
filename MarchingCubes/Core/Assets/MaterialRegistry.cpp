#include "pch.h"
#include "MaterialRegistry.h"
#include "TextureRegistry.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"

MaterialRegistry::MaterialRegistry(const InitInfo& info) :
	m_device(info.device),
	m_uploadContext(info.upload),
	m_descriptorAllocator(info.descriptorAllocator),
	m_textureRegistry(info.textureRegistry),
	m_rootSlot(info.materialRootSlot),
	m_descriptorSlot(m_descriptorAllocator->AllocateStaticSlot())
{}
MaterialRegistry::~MaterialRegistry() = default;

void MaterialRegistry::BuildTable(ID3D12GraphicsCommandList* cmd)
{
	if (m_materials.empty()) return;

	std::vector<MaterialConstants> constants;
	constants.reserve(m_materials.size());

	uint32_t textureBaseSlot = m_textureRegistry->GetDescriptorBaseSlot();
	for (const auto& src : m_materials)
	{
		MaterialConstants dst = src.GetConstants();

		uint32_t handle = src.GetDiffuseHandle();
		if (handle != UINT32_MAX && textureBaseSlot != UINT32_MAX)
		{
			const auto& texRes = m_textureRegistry->GetTexture(handle);
			dst.diffuse.texIndex = texRes.diffuseTexSlot - textureBaseSlot;
		}
		constants.push_back(dst);
	}

	m_materialBuffer.Reset();
	const UINT64 byteSize = static_cast<UINT64>(constants.size()) * sizeof(MaterialConstants);
	D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_materialBuffer)));
	m_uploadContext->UploadStructuredBuffer(cmd, constants.data(), byteSize, m_materialBuffer.Get(), 0);

	DescriptorHelper::CreateSRV_Structured(m_device, m_materialBuffer.Get(), static_cast<uint32_t>(sizeof(MaterialConstants)), m_descriptorAllocator->GetStaticCpu(m_descriptorSlot));
}

void MaterialRegistry::BindDescriptorTable(ID3D12GraphicsCommandList* cmd) const
{
	cmd->SetGraphicsRootDescriptorTable(m_rootSlot, m_descriptorAllocator->GetStaticGpu(m_descriptorSlot));
}

uint32_t MaterialRegistry::AddMaterial(const MaterialCPU& data)
{
	m_materials.push_back(data);
	return static_cast<uint32_t>(m_materials.size() - 1);
}

uint32_t MaterialRegistry::LoadMaterial(const std::wstring& path)
{
	// TODO : .mat 파일 로드 구현
	return UINT32_MAX;
}


