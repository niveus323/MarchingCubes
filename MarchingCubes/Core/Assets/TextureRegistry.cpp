#include "pch.h"
#include "TextureRegistry.h"
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

uint32_t TextureRegistry::LoadTexture(const std::wstring& path)
{
	// 이미 로드되어 있는지 확인
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_textures.size()); ++i)
	{
		if (m_textures[i].path == path)  return i;
	}

	std::unique_ptr<ScratchImage> loadedImg = std::make_unique<ScratchImage>();

	auto isDDS = [](const std::wstring& path) {
		return path.ends_with(L".dds");
	};

	auto isLinear = [](const std::wstring& path) {
		return false; // normal, roughness 테스트 시 구현
	};

	if (isDDS(path))
	{
		// DDS
		ThrowIfFailed(LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, nullptr, *loadedImg));
	}
	else
	{
		// WIC
		WIC_FLAGS flag = isLinear(path) ? WIC_FLAGS_FORCE_LINEAR : WIC_FLAGS_FORCE_SRGB;
		ThrowIfFailed(LoadFromWICFile(path.c_str(), flag, nullptr, *loadedImg));
	}
	
	const auto img = loadedImg->GetImages();
	size_t imgCount = loadedImg->GetImageCount();
	const TexMetadata& meta = loadedImg->GetMetadata();

	ComPtr<ID3D12Resource> res;
	ThrowIfFailed(DirectX::CreateTexture(m_device, meta, &res));

	uint32_t bindlessSlot = m_descriptorAllocator->AllocateStaticSlot();
	if (m_descriptorBaseSlot == UINT32_MAX)
		m_descriptorBaseSlot = bindlessSlot;

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
		// 필요 시 1D/3D/Array 등 추가 처리
	}

	auto cpuHandle = m_descriptorAllocator->GetStaticCpu(bindlessSlot);
	m_device->CreateShaderResourceView(res.Get(), &srvDesc, cpuHandle);

	// Lazy-Upload Queueing
	m_pendingTextures.push_back(PendingTextures{
		.dst = res.Get(), 
		.image = std::move(loadedImg), 
		.debugName = UTF16ToUTF8(path.c_str())
	});

	TextureResource result{
		.path = path,
		.meta = FinalizeMeta(res->GetDesc()),
		.res = std::move(res),
		.diffuseTexSlot = bindlessSlot
	};
	m_textures.push_back(result);
	return static_cast<uint32_t>(m_textures.size() - 1);
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
