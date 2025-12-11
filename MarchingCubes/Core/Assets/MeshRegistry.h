#pragma once
#include "MeshAsset.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include <unordered_map>

// Forward Declaration
class UploadContext;

class MeshRegistry
{
public:
    explicit MeshRegistry(UploadContext* uploadContext) : m_uploadContext(uploadContext)
    {}

    bool HasMesh(std::string_view key) const;
    void RegisterAsset(std::string_view key, std::unique_ptr<MeshAsset> asset);
    std::shared_ptr<Mesh> CreateMeshFromAsset(std::string_view key);
    const MeshAsset* GetAsset(const std::filesystem::path& path) const;

private:
    UploadContext* m_uploadContext;
    std::unordered_map<std::string, std::unique_ptr<MeshAsset>> m_assetCache;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_resourceCache;
};

