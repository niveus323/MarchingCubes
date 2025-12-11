#pragma once
#include "Core/DataStructures/Drawable.h"

struct ImportedMaterialDesc
{
    uint32_t internalMaterialIndex = 0;     // 에셋 파일 내부 material index
    std::string name;                  // material 이름

    // 텍스쳐 파일 경로들 (FBX에서 가져온 원본 경로 또는 리맵된 경로)
    std::wstring diffusePath;          // 알베도/베이스컬러
    std::wstring normalPath;           // 노멀
    std::wstring roughnessPath;        // 러프니스
    std::wstring metallicPath;         // 메탈릭
    std::wstring emissivePath;         // 에미시브

    DirectX::XMFLOAT3 diffuseColor = { 1,1,1 };

    float roughness = 0.5f;
    float metallic = 0.0f;
};

class MeshAsset
{
public:
    explicit MeshAsset(const std::filesystem::path& path, GeometryData&& geometry, std::vector<MeshSubmesh>&& submeshes, std::vector<ImportedMaterialDesc>&& materialDescs) : 
        m_sourcePath(path),
        m_data(std::move(geometry)),
        m_subMeshes(std::move(submeshes)),
        m_materialDescs(std::move(materialDescs))
    { }

    const GeometryData& GetGeometry() const { return m_data; }
    const std::vector<MeshSubmesh>& GetSubmesh() const { return m_subMeshes; }
    std::vector<MeshSubmesh>& GetSubmesh() { return m_subMeshes; }
    const auto& GetSourcePath() const { return m_sourcePath; }
    const auto& GetImportedMaterialDescs() const { return m_materialDescs; }

private:
    std::filesystem::path m_sourcePath;
    //std::filesystem::path m_cachePath;   //TODO : 에디터용 DDC(Derived Data Cache) 사용 시 활성화
    GeometryData m_data;
    std::vector<MeshSubmesh> m_subMeshes; // Material Slot 등 사전 정의된 추가 정보들
    std::vector<ImportedMaterialDesc> m_materialDescs;
};

