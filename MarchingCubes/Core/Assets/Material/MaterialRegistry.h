#pragma once
#include "Material.h"
#include <unordered_map>

class TextureRegistry;
class MaterialAsset;

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
public:
    MaterialRegistry(TextureRegistry* textureRegistry, uint32_t rootSlot = 3);
    ~MaterialRegistry();

    void SyncGpu(ID3D12GraphicsCommandList* cmd);
    void BindDescriptorTable(ID3D12GraphicsCommandList* cmd) const;

    uint32_t AddMaterial(const Material& data);
    uint32_t GetMaterialHandle(std::string_view path);
    uint32_t GetMaterialHandle(const std::shared_ptr<MaterialAsset> matAsset);
    const std::string GetMaterialPathByHandle(const uint32_t handle);

private:
    TextureRegistry* m_textureRegistry = nullptr;
    uint32_t m_rootSlot = 4;
    uint32_t m_descriptorSlot = UINT32_MAX;
    bool m_bDirty = false;

    std::vector<Material> m_materials;
    std::unordered_map<std::string, uint32_t> m_pathCache; // Path -> index 캐시

    ComPtr<ID3D12Resource> m_materialBuffer;
};

