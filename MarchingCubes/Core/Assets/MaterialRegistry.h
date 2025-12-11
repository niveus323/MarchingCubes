#pragma once
#include "Material.h"

class UploadContext;
class DescriptorAllocator;
class TextureRegistry;

class MaterialRegistry
{
public:
    struct InitInfo
    {
        ID3D12Device* device = nullptr;
        UploadContext* upload = nullptr;
        DescriptorAllocator* descriptorAllocator = nullptr;
        TextureRegistry* textureRegistry = nullptr;
        uint32_t materialRootSlot = 4;
    };
public:
    explicit MaterialRegistry(const InitInfo& info);
    ~MaterialRegistry();

    void BuildTable(ID3D12GraphicsCommandList* cmd);
    void BindDescriptorTable(ID3D12GraphicsCommandList* cmd) const;

    uint32_t AddMaterial(const Material& data);
    uint32_t LoadMaterial(const std::wstring& path);

private:
    ID3D12Device* m_device = nullptr;
    UploadContext* m_uploadContext = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    TextureRegistry* m_textureRegistry = nullptr;
    uint32_t m_rootSlot = 4;
    uint32_t m_descriptorSlot = UINT32_MAX;

    std::vector<Material> m_materials;
    ComPtr<ID3D12Resource> m_materialBuffer;
};

