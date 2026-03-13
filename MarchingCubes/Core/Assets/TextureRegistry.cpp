#include "pch.h"
#include "TextureRegistry.h"
#include "TextureAsset.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"

TextureRegistry::TextureRegistry()
{
	auto descriptorAllocator = EngineCore::GetDescriptorAllocator();
	assert(descriptorAllocator);
	constexpr uint32_t MAX_TEXTURES = 1024;
	m_descriptorBaseSlot = descriptorAllocator->AllocateStaticSlot(MAX_TEXTURES);
	m_textures.reserve(MAX_TEXTURES);
	m_freeIndices.reserve(MAX_TEXTURES);
}

TextureRegistry::~TextureRegistry() = default;

void TextureRegistry::SyncGpu(ID3D12GraphicsCommandList* cmd)
{
	for (auto& pendingTex : m_pendingTextures)
	{
		const auto* img = pendingTex.asset->GetImage()->GetImages();
		size_t imgCount = pendingTex.asset->GetImage()->GetImageCount();
		const DirectX::TexMetadata& meta = pendingTex.asset->GetMetadata();
		std::vector<D3D12_SUBRESOURCE_DATA> subres;
		ThrowIfFailed(DirectX::PrepareUpload(EngineCore::GetDevice(), img, imgCount, meta, subres));
		EngineCore::GetUploadContext()->UploadTexture(cmd, pendingTex.dst.Get(), subres, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, pendingTex.debugName.c_str());
		pendingTex.asset->ReleasePixelData(); //로드하여 GPU에 올렸으니 ScratchImage 객체 해제
	}

	m_pendingTextures.clear();
}

RegistryIndex TextureRegistry::FindTextureHandle(const std::string& path)
{
	auto iter = m_pathCache.find(path);
	if (iter != m_pathCache.end())
	{
		return iter->second;
	}

	return UINT32_MAX;
}

RegistryIndex TextureRegistry::LoadTexture(const std::shared_ptr<TextureAsset>& texAsset)
{
	ID3D12Device* device = EngineCore::GetDevice();
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();
	assert(device && descriptorAllocator);

	// 이 에셋이 이미 GPU에 업로드되어있는지 체크
	std::string key = texAsset->GetSourcePath().string();
	if (m_pathCache.contains(key)) return m_pathCache[key];

	RegistryIndex newHandle = UINT32_MAX;
	if (!m_freeIndices.empty())
	{
		newHandle = m_freeIndices.back();
		m_freeIndices.pop_back();
	}
	else
	{
		newHandle = static_cast<RegistryIndex>(m_textures.size());
		m_textures.push_back(TextureResource{});
	}

	const auto& img = texAsset->GetImage();
	const auto& meta = texAsset->GetMetadata();

	// GPU 리소스 생성
	ComPtr<ID3D12Resource> res;
	ThrowIfFailed(DirectX::CreateTexture(device, meta, &res));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format = meta.format,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
	};
	
	if (meta.dimension == TEX_DIMENSION_TEXTURE2D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(meta.mipLevels);
	}
	else
	{
		// TODO: 3D/Array 대응 필요 시 여기 확장
	}

	uint32_t descriptorSlot = m_descriptorBaseSlot + newHandle;	
	auto cpuHandle = descriptorAllocator->GetStaticCpu(descriptorSlot);
	device->CreateShaderResourceView(res.Get(), &srvDesc, cpuHandle);

	// Lazy-Upload Queueing
	m_pendingTextures.push_back(PendingTextures{
		.dst = res.Get(),
		.asset = texAsset,
		.debugName = texAsset->GetSourcePath().string()
	});

	m_pathCache.insert_or_assign(key, newHandle); // NOTE: Lazy-Upload 시 실패하는 케이스 존재 시, 실제 업로드 타이밍에 캐시 등록하도록 처리하고 중복 예약을 회피할 것

	m_textures[newHandle] = TextureResource{
		.path = texAsset->GetSourcePath().string(),
		.meta = FinalizeMeta(res->GetDesc()),
		.res = std::move(res),
		.descriptorSlot = descriptorSlot
	};
	return newHandle;
}

void TextureRegistry::UnloadTexture(RegistryIndex handle)
{
	if (handle >= m_textures.size() || !m_textures[handle].bValid)
		return;

	m_pathCache.erase(m_textures[handle].path);

	m_textures[handle].res.Reset();
	m_textures[handle].bValid = false;
	m_textures[handle].path = "";

	m_freeIndices.push_back(handle);
}

GPUArrayIndex TextureRegistry::GetTextureGPUIndex(RegistryIndex handle) const
{
	if (handle == UINT32_MAX || m_descriptorBaseSlot == UINT32_MAX)
		return UINT32_MAX;

	return m_textures[handle].descriptorSlot - m_descriptorBaseSlot;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureRegistry::GetGpuDescriptorHandle(RegistryIndex handle) const
{
	if (handle >= m_textures.size()) return { 0 }; // 유효하지 않은 핸들

	// 할당받은 descriptor 슬롯 번호를 이용해 DescriptorAllocator에서 실제 GPU 주소를 가져옴
	uint32_t slot = m_textures[handle].descriptorSlot;
	return EngineCore::GetDescriptorAllocator()->GetStaticGpu(slot);
}

TextureMeta TextureRegistry::FinalizeMeta(const D3D12_RESOURCE_DESC& desc)
{
	auto classifyResolution = [](UINT64 w, UINT h){
		if (w == 512 && h == 512)   return TextureResolutionTier::_512;
		if (w == 1024 && h == 1024)  return TextureResolutionTier::_1k;
		if (w == 2048 && h == 2048)  return TextureResolutionTier::_2k;
		if (w == 4096 && h == 4096)  return TextureResolutionTier::_4k;

		return TextureResolutionTier::Unknown;
	};

	TextureResolutionTier tier = classifyResolution(desc.Width, desc.Height);
	return TextureMeta{
		.tier = tier,
		.format = desc.Format,
		.width = static_cast<uint32_t>(desc.Width),
		.height = desc.Height,
		.mipLevels = desc.MipLevels,
		.canBeInArray = (tier != TextureResolutionTier::Unknown)
	};
}
