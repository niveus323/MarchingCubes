#include "pch.h"
#include "ResourceManager.h"
#include "Core/Utils/FileUtils.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Engine/Serializer/JsonSerializer.h"
#include <ranges>

ResourceManager::ResourceManager()
{
	m_fbxImporter = std::make_unique<FBXImporter>();

	LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/DefaultFilled.json"));
	LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/DefaultLine.json"));
	InitializePrimitives();
}

ResourceManager::~ResourceManager()
{
}

std::shared_ptr<TextureAsset> ResourceManager::LoadTextureAsset(const std::filesystem::path & fileDir, const std::filesystem::path & folderDir)
{
	// 이미 로드되어 있는지 확인
	std::string key = fileDir.string();
	if (m_textureAssetCache.contains(key))
	{
		return m_textureAssetCache[key];
	}

	std::filesystem::path fullPath = fileDir;
	if (!fullPath.is_absolute())
	{
		fullPath = folderDir / fileDir;
	}

	auto texAsset = std::make_shared<TextureAsset>(fullPath); 
	m_textureAssetCache.insert_or_assign(key, texAsset);
	return texAsset;
}

std::shared_ptr<MaterialAsset> ResourceManager::LoadMaterialAsset(const std::filesystem::path& path)
{
	std::string key = path.string();
	if (m_materialAssetCache.contains(key))
	{
		return m_materialAssetCache[key];
	}
	
	std::filesystem::path fullPath = path;
	if (!fullPath.is_absolute())
	{
		fullPath = std::filesystem::path(ASSETS_ROOT) / path;
	}

	JsonSerializer ar(false);
	ar.LoadFromFile(fullPath.string());
	auto asset = SerializeMaterialAsset(ar);
	asset->m_assetPath = path.string();
	m_materialAssetCache.insert_or_assign(key, asset);
	return asset;
}

std::shared_ptr<MeshAsset> ResourceManager::LoadMeshAsset(const std::filesystem::path& path, const MeshImportOptions& options)
{
	std::string key = path.string();

	// 캐시된 에셋없으면 등록
	if (m_meshAssetCache.contains(key))
	{
		return m_meshAssetCache[key];
	}

	std::filesystem::path fullPath = path;
	if (!fullPath.is_absolute())
	{
		fullPath = std::filesystem::path(ASSETS_ROOT) / path;
	}

	// .meta 파일이 있으면 해당 옵션을 우선적하고, 없을 때 option값을 적용 (TODO : 리임포트 대응하기)
	MeshImportOptions finalOptions = options;
	if (std::filesystem::exists(fullPath.string() + ".meta")) finalOptions = LoadMetaOptions(fullPath);
	else SaveMetaOptions(fullPath, options);

	auto importData = m_fbxImporter->LoadFile(fullPath, finalOptions);
	if (!importData.success)
	{
		Log::Print(ELogVerbosity::Fatal, "ResourceManager", "Failed to load mesh file: {}", key);
		return nullptr;
	}

	// MaterialAsset 체크 및 생성 
	std::vector<std::shared_ptr<MaterialAsset>> materialAssets;
	for (auto& importedMats : importData.materials)
	{
		std::string matKey = importedMats.name;
		if (m_materialAssetCache.contains(matKey))
		{
			materialAssets.push_back(m_materialAssetCache[matKey]);
			continue;
		}

		std::shared_ptr<MaterialAsset> newMatAsset = std::make_shared<MaterialAsset>();
		newMatAsset->m_assetPath = importedMats.name;
		newMatAsset->m_constants = MaterialConstants{
			.albedo = importedMats.diffuseColor,
			.metallic = importedMats.metallic,
			.roughness = importedMats.roughness
		};

		if (!importedMats.diffusePath.empty())	 newMatAsset->m_diffuse = LoadTextureAsset(ResolveTexturePath(path, importedMats.diffusePath));
		if (!importedMats.emissivePath.empty())	 newMatAsset->m_emissive = LoadTextureAsset(ResolveTexturePath(path, importedMats.emissivePath));
		if (!importedMats.metallicPath.empty())	 newMatAsset->m_metallic = LoadTextureAsset(ResolveTexturePath(path, importedMats.metallicPath));
		if (!importedMats.normalPath.empty())	 newMatAsset->m_normal = LoadTextureAsset(ResolveTexturePath(path, importedMats.normalPath));
		if (!importedMats.roughnessPath.empty()) newMatAsset->m_roughness = LoadTextureAsset(ResolveTexturePath(path, importedMats.roughnessPath));

		materialAssets.push_back(newMatAsset);
		m_materialAssetCache.insert_or_assign(matKey, std::move(newMatAsset));
	}

	auto asset = std::make_shared<MeshAsset>(path, std::move(importData.geometry), std::move(importData.submeshes), std::move(materialAssets));
	m_meshAssetCache[key] = asset;
	return asset;
}

std::shared_ptr<MeshAsset> ResourceManager::GetMeshAsset(const std::filesystem::path& path) const
{
	auto it = m_meshAssetCache.find(path.string());
	if (it != m_meshAssetCache.end()) return it->second;
	return nullptr;
}

std::shared_ptr<DataAsset> ResourceManager::LoadDataAsset(const std::filesystem::path& path)
{
	std::string key = path.string();

	auto it = m_dataAssetCache.find(key);
	if (it != m_dataAssetCache.end())
	{
		return it->second;
	}

	auto asset = DataAsset::Load(path);
	if (asset)
	{
		m_dataAssetCache[key] = asset;
	}
	else
	{
		Log::Print(ELogVerbosity::Fatal, "ResourceManager", "Failed to load DataAsset: {}", key);
	}

	return asset;
}

bool ResourceManager::SaveDataAsset(const std::filesystem::path& path, std::shared_ptr<DataAsset> asset)
{
	m_dataAssetCache[path.string()] = asset;
	return asset->Save(path);
}

std::vector<std::string> ResourceManager::GetCachedList(EAssetType type) const
{
	switch (type)
	{
		case EAssetType::DataAsset:
		{
			auto keys_view = m_dataAssetCache | std::views::keys;
			return std::vector<std::string>(keys_view.begin(), keys_view.end());
		}
		break;
		case EAssetType::MeshAsset:
		{
			auto keys_view = m_meshAssetCache | std::views::keys;
			return std::vector<std::string>(keys_view.begin(), keys_view.end());
		}
		break;
		case EAssetType::TextureAsset:
		{
			auto keys_view = m_textureAssetCache | std::views::keys;
			return std::vector<std::string>(keys_view.begin(), keys_view.end());
		}
		break;
		case EAssetType::MaterialAsset:
		{
			auto keys_view = m_materialAssetCache | std::views::keys;
			return std::vector<std::string>(keys_view.begin(), keys_view.end());
		}
		break;
		default:
		break;
	}

	return std::vector<std::string>();
}

std::filesystem::path ResourceManager::ResolveTexturePath(const std::filesystem::path& fbxPath, const std::filesystem::path& texRelativePath)
{
	if (texRelativePath.empty()) return "";

	// 그대로 존재하면 사용 (절대 경로 등)
	if (std::filesystem::exists(texRelativePath)) return texRelativePath;

	// FBX 파일과 같은 폴더 확인
	auto fileName = texRelativePath.filename();
	std::filesystem::path siblingPath = fbxPath.parent_path() / fileName;
	if (std::filesystem::exists(siblingPath)) return siblingPath;

	// 텍스쳐 확인(Assets/Textures/...)
	auto parentDir = fbxPath.parent_path().filename();
	auto contentsRelativePath = parentDir / fileName;
	auto texturePath = GetFullPath(AssetType::Texture, contentsRelativePath.c_str());
	if (std::filesystem::exists(texturePath)) return texturePath;

	// 못 찾았으면 원본 이름만 리턴 (TextureRegistry가 실패 처리하거나 기본 텍스처 사용)
	return fileName;
}

void ResourceManager::InitializePrimitives()
{
	// CPU에서 메쉬 데이터를 생성하는 Primitive 메쉬는 사전에 정의된 타입 별로 미리 로드해두고 꺼내서 사용
	auto registerPrimitive = [&](const std::string& key, GeometryData&& data, std::shared_ptr<MaterialAsset> mat) {
		std::vector<MeshSubmesh> submeshes;
		MeshSubmesh sm;
		sm.indexCount = (uint32_t)data.indices.size();
		sm.materialslot = 0;
		submeshes.push_back(sm);

		std::vector<std::shared_ptr<MaterialAsset>> mats;
		mats.push_back(mat);
		auto asset = std::make_shared<MeshAsset>(key, std::move(data), std::move(submeshes), std::move(mats));
		m_meshAssetCache[key] = asset;
		};

	auto filledMat = LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/DefaultFilled.json"));
	// Solid Sphere
	registerPrimitive("@SolidSphere", MeshGenerator::CreateSphereMeshData(), filledMat);
	// Solid Cube
	registerPrimitive("@SolidCube", MeshGenerator::CreateSolidCube(), filledMat);
	// Billboard Quad
	registerPrimitive("@BillboardQuad", MeshGenerator::CreateSolidQuad(), filledMat);

	auto LineMat = LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/DefaultLine.json"));
	// Wire Sphere
	registerPrimitive("@WireSphere", MeshGenerator::CreateWireSphere(), LineMat);
	// Wire Cube
	registerPrimitive("@WireCube", MeshGenerator::CreateWireCube(), LineMat);
}

MeshImportOptions ResourceManager::LoadMetaOptions(const std::filesystem::path& assetPath)
{
	std::filesystem::path metaPath = assetPath.string() + ".meta";
	MeshImportOptions ops{};

	nlohmann::json j;
	if (FileUtils::ReadJSON(metaPath, j))
	{
		if (j.contains("bUnitConversion")) ops.bUnitConversion = j["bUnitConversion"];
		if (j.contains("uniformScale")) ops.uniformScale = j["uniformScale"];
	}
	else
	{
		// 메타 파일이 없거나 읽기 실패 시 기본값 저장
		SaveMetaOptions(assetPath, ops);
	}

	return ops;
}

void ResourceManager::SaveMetaOptions(const std::filesystem::path& assetPath, const MeshImportOptions& options)
{
	std::filesystem::path metaPath = assetPath.string() + ".meta";

	nlohmann::json j;
	j["bUnitConversion"] = options.bUnitConversion;
	j["uniformScale"] = options.uniformScale;

	FileUtils::WriteJSON(metaPath, j);
}

std::shared_ptr<MaterialAsset> ResourceManager::SerializeMaterialAsset(Serializer& ar)
{
	auto asset = std::make_shared<MaterialAsset>();
	ar.Serialize("PSO", asset->m_psoName);
	MaterialConstants consts{};
	ar.BeginObject("MaterialConstants");
	ar.Serialize("Albedo", consts.albedo);
	ar.Serialize("Metallic", consts.metallic);
	ar.Serialize("Roughness", consts.roughness);
	ar.Serialize("SpecularStrength", consts.specularStrength);
	ar.Serialize("AO", consts.ao);
	ar.Serialize("IOR", consts.ior);
	ar.Serialize("Opacity", consts.opacity);

	// Enum은 직접 직렬화를 지원하지 않으므로 캐스팅하여 처리
	uint32_t shadingModelVal = static_cast<uint32_t>(consts.shadingModel);
	ar.Serialize("ShadingModel", shadingModelVal);
	if(!ar.IsSaving()) consts.shadingModel = static_cast<EShadingModel>(shadingModelVal);

	ar.BeginObject("TextureParams");
	uint32_t mappingVal = static_cast<uint32_t>(consts.baseTextures.mappingType);
	ar.Serialize("TextureMappingType", mappingVal);
	if (!ar.IsSaving()) consts.baseTextures.mappingType = static_cast<ETextureMappingTypes>(mappingVal);

	TriplanarParams triParams{};
	ar.BeginObject("TriplanarParams");
	ar.Serialize("Scale", triParams.scale);
	ar.Serialize("Sharpness", triParams.sharpness);
	ar.EndObject();
	consts.baseTextures.triplanar = triParams;
	ar.EndObject();

	ar.EndObject();
	asset->m_constants = consts;

	auto SerializeTexture = [&](const std::string& field) -> std::shared_ptr<TextureAsset> {
		std::string path = "";
		ar.Serialize(field, path);
		if (path.empty()) return nullptr;

		return LoadTextureAsset(GetFullPath(AssetType::Texture, std::filesystem::path(path).c_str()));
		//return LoadTextureAsset(std::filesystem::path(path).c_str());
	};

	asset->m_diffuse = SerializeTexture("Diffuse");
	asset->m_normal = SerializeTexture("Normal");
	asset->m_arm = SerializeTexture("ARM");
	asset->m_displacement = SerializeTexture("Displacement");
	asset->m_roughness = SerializeTexture("Roughness");
	asset->m_emissive = SerializeTexture("Emissive");
	asset->m_metallic = SerializeTexture("Metallic");
	return asset;
}