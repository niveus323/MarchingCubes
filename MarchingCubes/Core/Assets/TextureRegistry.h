#pragma once
#include <filesystem>
#include <DirectXTex.h>

// Forward Delclaration
class UploadContext;
class DescriptorAllocator;
class TextureAsset;

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

/* [TextureRegistry]
* - LifeTime : RenderSystem Load -> UnLoad
* - OwnerShip : RenderSystem
* - Access : ResourceManager::GetTextureRegistry
* - Responsibility :
*   - Texture Upload : Texture CPU->GPU 업로드 및 관리
*/
class TextureRegistry
{
public:
	TextureRegistry(uint32_t rootSlot = 5);
	~TextureRegistry();

	void SyncGpu(ID3D12GraphicsCommandList* cmd);
	void BindDescriptorTable(ID3D12GraphicsCommandList* cmd);

    uint32_t GetTextureHandle(const std::string& path);
	uint32_t LoadTexture(const std::shared_ptr<TextureAsset>& texAsset);
	const TextureResource& GetTexture(size_t texHandle) const { return m_textures[texHandle]; }
    uint32_t GetDescriptorBaseSlot() const { return m_descriptorBaseSlot; }
    uint32_t GetBindlessIndex(uint32_t handle) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t handle) const;

private:
    TextureMeta FinalizeMeta(const D3D12_RESOURCE_DESC& desc);

private:
    uint32_t m_rootSlot = 5;
    uint32_t m_descriptorBaseSlot = UINT32_MAX;

	std::vector<TextureResource> m_textures;
    std::unordered_map<std::string, uint32_t> m_pathCache;

    // Lazy-Upload
	struct PendingTextures
	{
		ComPtr<ID3D12Resource> dst;
        std::shared_ptr<TextureAsset> asset;
        std::string debugName = "";
	};
	std::vector<PendingTextures> m_pendingTextures;

};

