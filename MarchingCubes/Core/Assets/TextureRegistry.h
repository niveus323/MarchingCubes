#pragma once
#include <filesystem>
#include <DirectXTex.h>
using RegistryIndex = uint32_t;
using GPUArrayIndex = uint32_t;

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
    std::string path;
    TextureMeta meta;
    ComPtr<ID3D12Resource> res;
    bool bValid = true;

    uint32_t descriptorSlot = UINT32_MAX;
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
	TextureRegistry();
	~TextureRegistry();

	void SyncGpu(ID3D12GraphicsCommandList* cmd);

    RegistryIndex FindTextureHandle(const std::string& path);
    RegistryIndex LoadTexture(const std::shared_ptr<TextureAsset>& texAsset);
    void UnloadTexture(RegistryIndex handle);
	const TextureResource& GetTexture(size_t texHandle) const { return m_textures[texHandle]; }
    GPUArrayIndex GetTextureGPUIndex(RegistryIndex handle) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(RegistryIndex handle) const;

private:
    TextureMeta FinalizeMeta(const D3D12_RESOURCE_DESC& desc);

private:
    uint32_t m_descriptorBaseSlot = UINT32_MAX;

	std::vector<TextureResource> m_textures;
    std::vector<RegistryIndex> m_freeIndices;
    std::unordered_map<std::string, RegistryIndex> m_pathCache;

    // Lazy-Upload
	struct PendingTextures
	{
		ComPtr<ID3D12Resource> dst;
        std::shared_ptr<TextureAsset> asset;
        std::string debugName = "";
	};
	std::vector<PendingTextures> m_pendingTextures;

};

