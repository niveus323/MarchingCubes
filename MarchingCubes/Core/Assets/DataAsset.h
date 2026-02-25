#pragma once
#include <vector>
#include <string>
#include <filesystem>

enum class EDataAssetType : uint32_t
{
    None = 0,
    TerrainSDF = 1,
    Custom = 99
};

// 모든 바이너리 파일의 맨 앞에 붙을 공통 헤더
struct AssetHeader
{
    uint32_t magic = 0x44415441; // "DATA"
    EDataAssetType type = EDataAssetType::None;
    uint32_t version = 1;
    uint64_t dataSize = 0;
};

/* [DataAsset]
* - LifeTime : Asset Load -> Asset UnLoad
* - OwnerShip : ResourceManager
* - Access : ResourceManager::LoadDataAsset
*/
class DataAsset
{
public:
    DataAsset() = default;
    DataAsset(EDataAssetType type, const void* data, size_t size);

    void SetData(EDataAssetType type, const void* data, size_t size);

    // 데이터 접근
    EDataAssetType GetType() const { return m_header.type; }
    size_t GetSize() const { return m_data.size(); }
    const uint8_t* GetRawData() const { return m_data.data(); }
    const AssetHeader& GetHeader() const { return m_header; }

    static std::shared_ptr<DataAsset> Load(const std::filesystem::path& path);
    bool Save(const std::filesystem::path& path);

private:
    AssetHeader m_header;
    std::vector<uint8_t> m_data; // Raw Blob
};