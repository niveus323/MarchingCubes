#include "pch.h"
#include "TextureRegistry.h"
#include "TextureAsset.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"

TextureRegistry::TextureRegistry(uint32_t rootSlot) :
	m_rootSlot(rootSlot)
{
	auto descriptorAllocator = EngineCore::GetDescriptorAllocator();
	assert(descriptorAllocator);

	m_descriptorBaseSlot = descriptorAllocator->AllocateStaticSlot();
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

void TextureRegistry::BindDescriptorTable(ID3D12GraphicsCommandList* cmd)
{
	if (m_descriptorBaseSlot == UINT32_MAX) return;

	cmd->SetGraphicsRootDescriptorTable(m_rootSlot, EngineCore::GetDescriptorAllocator()->GetStaticGpu(m_descriptorBaseSlot));
}

uint32_t TextureRegistry::GetTextureHandle(const std::string& path)
{
	auto iter = m_pathCache.find(path);
	if (iter != m_pathCache.end())
	{
		return iter->second;
	}

	return UINT32_MAX;
}

uint32_t TextureRegistry::LoadTexture(const std::shared_ptr<TextureAsset>& texAsset)
{
	ID3D12Device* device = EngineCore::GetDevice();
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();
	assert(device && descriptorAllocator);

	// 이 에셋이 이미 GPU에 업로드되어있는지 체크
	std::string key = texAsset->GetSourcePath().string();
	if (m_pathCache.contains(key)) return m_pathCache[key];

	const auto& img = texAsset->GetImage();
	const auto& meta = texAsset->GetMetadata();

	// GPU 리소스 생성
	ComPtr<ID3D12Resource> res;
	ThrowIfFailed(DirectX::CreateTexture(device, meta, &res));

	uint32_t bindlessSlot = descriptorAllocator->AllocateStaticSlot();
	if (m_descriptorBaseSlot == UINT32_MAX) m_descriptorBaseSlot = bindlessSlot;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = meta.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (meta.dimension == TEX_DIMENSION_TEXTURE2D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(meta.mipLevels);
	}
	else
	{
		// TODO: 3D/Array 대응 필요 시 여기 확장
	}

	auto cpuHandle = descriptorAllocator->GetStaticCpu(bindlessSlot);
	device->CreateShaderResourceView(res.Get(), &srvDesc, cpuHandle);

	// Lazy-Upload Queueing
	m_pendingTextures.push_back(PendingTextures{
		.dst = res.Get(),
		.asset = texAsset,
		.debugName = texAsset->GetSourcePath().string()
	});

	m_pathCache.insert_or_assign(key, m_textures.size()); // NOTE: Lazy-Upload 시 실패하는 케이스 존재 시, 실제 업로드 타이밍에 캐시 등록하도록 처리하고 중복 예약을 회피할 것
	TextureResource result{
		.path = texAsset->GetSourcePath(),
		.meta = FinalizeMeta(res->GetDesc()),
		.res = std::move(res),
		.bindlessSlot = bindlessSlot
	};
	m_textures.push_back(std::move(result));
	return static_cast<uint32_t>(m_textures.size() - 1);
}

uint32_t TextureRegistry::GetBindlessIndex(uint32_t handle) const
{
	if (handle == UINT32_MAX || m_descriptorBaseSlot == UINT32_MAX)
		return UINT32_MAX;

	const auto& texRes = GetTexture(static_cast<size_t>(handle));
	return texRes.bindlessSlot - m_descriptorBaseSlot;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureRegistry::GetGpuHandle(uint32_t handle) const
{
	if (handle >= m_textures.size()) return { 0 }; // 유효하지 않은 핸들

	// 할당받은 bindless 슬롯 번호를 이용해 DescriptorAllocator에서 실제 GPU 주소를 가져옴
	uint32_t slot = m_textures[handle].bindlessSlot;
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
