#include "pch.h"
#include "TextureRegistry.h"
#include "TextureAsset.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"

TextureRegistry::TextureRegistry(const InitInfo& initInfo) :
	m_device(initInfo.device),
	m_uploadContext(initInfo.upload),
	m_descriptorAllocator(initInfo.descriptorAllocator),
	m_rootSlot(initInfo.rootSlot),
	m_descriptorBaseSlot(UINT32_MAX)
{
}

TextureRegistry::~TextureRegistry() = default;

void TextureRegistry::syncGpu(ID3D12GraphicsCommandList* cmd)
{
	for (auto& pendingTex : m_pendingTextures)
	{
		const auto* img = pendingTex.image->GetImages();
		size_t imgCount = pendingTex.image->GetImageCount();
		const DirectX::TexMetadata& meta = pendingTex.image->GetMetadata();

		std::vector<D3D12_SUBRESOURCE_DATA> subres;
		ThrowIfFailed(DirectX::PrepareUpload(m_device, img, imgCount, meta, subres));
		m_uploadContext->UploadTexture(cmd, pendingTex.dst.Get(), subres, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, pendingTex.debugName.c_str());
	}

	m_pendingTextures.clear();
}

void TextureRegistry::BindDescriptorTable(ID3D12GraphicsCommandList* cmd)
{
	if (m_descriptorBaseSlot == UINT32_MAX) return;

	cmd->SetGraphicsRootDescriptorTable(m_rootSlot, m_descriptorAllocator->GetStaticGpu(m_descriptorBaseSlot));
}

uint32_t TextureRegistry::LoadTexture(const std::filesystem::path& logicalPath)
{
	// 이미 로드되어 있는지 확인
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_textures.size()); ++i)
	{
		if (m_textures[i].path == logicalPath) return i;
	}
	TextureAsset asset(logicalPath);

	const auto& img = asset.GetImage();
	const auto& meta = asset.GetMetadata();

	// GPU 리소스 생성
	ComPtr<ID3D12Resource> res;
	ThrowIfFailed(DirectX::CreateTexture(m_device, meta, &res));

	uint32_t bindlessSlot = m_descriptorAllocator->AllocateStaticSlot();
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
		// TODO: 1D/3D/Array 대응 필요 시 여기 확장
	}

	auto cpuHandle = m_descriptorAllocator->GetStaticCpu(bindlessSlot);
	m_device->CreateShaderResourceView(res.Get(), &srvDesc, cpuHandle);

	// Lazy-Upload Queueing
	m_pendingTextures.push_back(PendingTextures{
		.dst = res.Get(),
		.image = asset.ExtractImage(),
		.debugName = UTF16ToUTF8(logicalPath.c_str())
		});

	TextureResource result{
		.path = logicalPath,
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
