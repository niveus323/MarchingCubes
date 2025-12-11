#pragma once
#include <filesystem>
#include <DirectXTex.h>

// Forward Delclaration
class UploadContext;
class DescriptorAllocator;

enum class TextureResolutionTier : uint8_t
{
    Unknown = 0,
    _512,
    _1k,
    _2k,
    _4k,
};

struct TextureMeta
{
    TextureResolutionTier tier = TextureResolutionTier::Unknown;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 0;

    bool canBeInArray = false;
};

struct TextureResource
{
    std::wstring path;
    TextureMeta meta;
    ComPtr<ID3D12Resource> res;

    uint32_t bindlessSlot = UINT32_MAX;
};


class TextureRegistry
{
public:
    struct InitInfo {
        ID3D12Device* device = nullptr;
        UploadContext* upload = nullptr;
        DescriptorAllocator* descriptorAllocator = nullptr;
        uint32_t rootSlot = 6;
    };

	explicit TextureRegistry(const InitInfo& initInfo);
	~TextureRegistry();

	void syncGpu(ID3D12GraphicsCommandList* cmd);
	void BindDescriptorTable(ID3D12GraphicsCommandList* cmd);

	uint32_t LoadTexture(const std::filesystem::path& logicalPath);
	const TextureResource& GetTexture(size_t texHandle) const { return m_textures[texHandle]; }
    uint32_t GetDescriptorBaseSlot() const { return m_descriptorBaseSlot; }
    uint32_t GetBindlessIndex(uint32_t handle) const;

private:
    TextureMeta FinalizeMeta(const D3D12_RESOURCE_DESC& desc);

private:
    ID3D12Device* m_device = nullptr;
    UploadContext* m_uploadContext = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    uint32_t m_rootSlot = 6;
    uint32_t m_descriptorBaseSlot = UINT32_MAX;

	std::vector<TextureResource> m_textures;

    // Lazy-Upload
	struct PendingTextures
	{
		ComPtr<ID3D12Resource> dst;
		std::unique_ptr<DirectX::ScratchImage> image;
        std::string debugName = "";
	};
	std::vector<PendingTextures> m_pendingTextures;
};

