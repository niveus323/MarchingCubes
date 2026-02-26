#include "pch.h"
#include "DataAsset.h"
#include "Core/Utils/FileUtils.h"

DataAsset::DataAsset(EDataAssetType type, const void* data, size_t size)
{
    SetData(type, data, size);
}

void DataAsset::SetData(EDataAssetType type, const void* data, size_t size)
{
    m_header.type = type;
    m_header.dataSize = size;
    m_data.resize(size);
    if (size > 0 && data)
    {
        std::memcpy(m_data.data(), data, size);
    }
}

std::shared_ptr<DataAsset> DataAsset::Load(const std::filesystem::path& path)
{
    std::vector<uint8_t> buffer;
    if (!FileUtils::BinaryFile::Readfile(path, buffer)) return nullptr;

    if (buffer.size() < sizeof(AssetHeader))
    {
        Log::Print("DataAsset", "Invalid Header. DataAsset file : %s", path.string().c_str());
        return nullptr;
    }

    auto asset = std::make_shared<DataAsset>();

    std::memcpy(&asset->m_header, buffer.data(), sizeof(AssetHeader));

    // 실제 데이터 크기가 헤더에 기록된 크기와 일치하는지(혹은 충분한지) 확인
    if (buffer.size() < sizeof(AssetHeader) + asset->m_header.dataSize)
    {
        Log::Print("DataAsset", "DataAsset corruption detected (size mismatch): %s", path.string().c_str());
        return nullptr;
    }

    asset->m_data.resize(asset->m_header.dataSize);
    if (asset->m_header.dataSize > 0)
    {
        std::memcpy(asset->m_data.data(), buffer.data() + sizeof(AssetHeader), asset->m_header.dataSize);
    }

    return asset;
}

bool DataAsset::Save(const std::filesystem::path& path)
{
    std::vector<FileUtils::BinaryFile::DataChunk> chunks = {
        FileUtils::BinaryFile::DataChunk{ // 헤더
            .ptr = &m_header, 
            .size = sizeof(AssetHeader) 
        },
        FileUtils::BinaryFile::DataChunk{ // 데이터
            .ptr = m_data.data(),
            .size = m_data.size()
        }
    };
    return FileUtils::BinaryFile::WriteFile(path, chunks);
}
