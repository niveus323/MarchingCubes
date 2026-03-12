#pragma once
#include "ThirdParty/FBXImporter.h"
#include "Material/MaterialAsset.h"
#include "MeshAsset.h"
#include "TextureAsset.h"
#include "DataAsset.h"
#include <unordered_map>

enum class EAssetType
{
	DataAsset,
	MeshAsset,
	TextureAsset,
	MaterialAsset
};

/* [ResourceManager]
* - LifeTime : Engine Load -> Engine UnLoad
* - OwnerShip : Engine
* - Access : Engine::GetResourceManager()
* - Responsibility : 
* 	- MeshData : .obj, .fbx 파일을 로드하여 소유/참조 전달
* 	- AnimData : .anim 파일을 로드하여 소유/참조 전달
* 	- SoundData : .wav 등 사운드 파일을 로드하여 소유/참조 전달
* 	- Texture : .dds, .png 등 텍스쳐 파일을 로드하여 TextureAsset을 소유/참조 전달
* 	- SceneData : .scene 과 같이 에디터로 작업한 내역을 저장 + 로드하여 SceneData를 소유/참조 전달
* 	- DataAsset : .xml, .tex 등 raw 데이터를 관리/참조 전달
*/
class Serializer;

class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();

	// --- Texture Asset ---
	std::shared_ptr<TextureAsset> LoadTextureAsset(const std::filesystem::path& fileDir, const std::filesystem::path& folderDir = std::filesystem::path(L"Assets/Textures"));
	
	// --- Material Asset ---
	std::shared_ptr<MaterialAsset> LoadMaterialAsset(const std::filesystem::path& path);
	
	// --- Mesh Asset ---
	std::shared_ptr<MeshAsset> LoadMeshAsset(const std::filesystem::path& path, const MeshImportOptions& options);
	std::shared_ptr<MeshAsset> GetMeshAsset(const std::filesystem::path& path) const;

	// --- Data Asset ---
	std::shared_ptr<DataAsset> LoadDataAsset(const std::filesystem::path& path);
	bool SaveDataAsset(const std::filesystem::path & path, std::shared_ptr<DataAsset> asset);

	// --- Cached List ---
	std::vector<std::string> GetCachedList(EAssetType type) const;

	static std::filesystem::path ResolveTexturePath(const std::filesystem::path& fbxPath, const std::filesystem::path& texRelativePath);

private:
	void InitializePrimitives();
	MeshImportOptions LoadMetaOptions(const std::filesystem::path& assetPath);
	void SaveMetaOptions(const std::filesystem::path& assetPath, const MeshImportOptions& options);

	//Serialize
	std::shared_ptr<MaterialAsset> SerializeMaterialAsset(Serializer& ar);

private:
	std::unique_ptr<FBXImporter> m_fbxImporter;

	// Cache
	std::unordered_map<std::string, std::shared_ptr<DataAsset>> m_dataAssetCache;
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> m_meshAssetCache;
	std::unordered_map<std::string, std::shared_ptr<MaterialAsset>> m_materialAssetCache;
	std::unordered_map<std::string, std::shared_ptr<TextureAsset>> m_textureAssetCache;
	
};

