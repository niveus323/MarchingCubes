#pragma once
#include "Material.h"
#include <unordered_map>
using RegistryIndex = uint32_t;
using GPUArrayIndex = uint32_t;

class TextureRegistry;
class MaterialAsset;
class TextureAsset;
struct MaterialInstance;

/* [MaterialRegistry]
* - LifeTime : RenderSystem Load -> UnLoad
* - OwnerShip : RenderSystem
* - Access : ResourceManager::GetMaterialRegistry
* - Responsibility :
*   - Material Upload : Materal 버퍼 관리
*/
class MaterialRegistry
{
public:
    MaterialRegistry(TextureRegistry* textureRegistry);
    ~MaterialRegistry();

    void SyncGpu(ID3D12GraphicsCommandList* cmd);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle() const;

    RegistryIndex RegisterMaterialInstance(const MaterialInstance& matInstance, const std::string& instanceKey);
    RegistryIndex RegisterMaterialAsset(const std::shared_ptr<MaterialAsset> matAsset);
    RegistryIndex FindMaterialHandle(std::string_view path);
    const std::string GetMaterialPath(const RegistryIndex handle);
private:
    GPUArrayIndex ResolveTextureAsset(const std::shared_ptr<TextureAsset>& asset) const;
    
private:
    TextureRegistry* m_textureRegistry = nullptr;
    uint32_t m_descriptorSlot = UINT32_MAX;
    bool m_bDirty = false;

    std::vector<Material> m_materials;
    std::unordered_map<std::string, RegistryIndex> m_pathCache; // Path -> RegistryIndex 캐시

    ComPtr<ID3D12Resource> m_materialBuffer;
    std::vector<ComPtr<ID3D12Resource>> m_pendingKills;
};

