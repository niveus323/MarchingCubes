#include "pch.h"
#include "MeshRegistry.h"

bool MeshRegistry::HasMesh(std::string_view key) const
{
	return m_assetCache.contains(key.data());
}

void MeshRegistry::RegisterAsset(std::string_view key, std::unique_ptr<MeshAsset> asset)
{
	if (asset)
	{
		m_assetCache[key.data()] = std::move(asset);
	}
}

std::shared_ptr<Mesh> MeshRegistry::CreateMeshFromAsset(std::string_view key)
{
	// 이미 GPU 리소스가 있으면 반환
	auto resIt = m_resourceCache.find(key.data());
	if (resIt != m_resourceCache.end()) return resIt->second;

	// Asset 데이터 찾기
	auto assetIt = m_assetCache.find(key.data());
	if (assetIt == m_assetCache.end()) return nullptr;

	const MeshAsset* asset = assetIt->second.get();
	auto mesh = std::make_shared<Mesh>(m_uploadContext, asset->GetGeometry(), asset->GetSubmesh(), std::filesystem::path(key).filename().string());
	m_resourceCache[key.data()] = mesh;

	return mesh;
}

const MeshAsset* MeshRegistry::GetAsset(const std::filesystem::path& path) const
{
	auto it = m_assetCache.find(path.string());
	if (it != m_assetCache.end())
	{
		return it->second.get();
	}
	return nullptr;
}