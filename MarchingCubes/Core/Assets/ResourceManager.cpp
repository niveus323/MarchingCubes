#include "pch.h"
#include "ResourceManager.h"
#include "TextureRegistry.h"
#include "MaterialRegistry.h"
#include "MeshRegistry.h"

ResourceManager::ResourceManager(ID3D12Device* device, UploadContext* uploadContext, DescriptorAllocator* descriptorAllocator) :
	m_device(device),
	m_uploadContext(uploadContext),
	m_descriptorAllocator(descriptorAllocator)
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	m_textureRegistry = std::make_unique<TextureRegistry>(TextureRegistry::InitInfo{
		.device = m_device,
		.upload = m_uploadContext,
		.descriptorAllocator = m_descriptorAllocator
	});
	m_materialRegistry = std::make_unique<MaterialRegistry>(MaterialRegistry::InitInfo{
		.device = m_device,
		.upload = m_uploadContext,
		.descriptorAllocator = m_descriptorAllocator,
		.textureRegistry = m_textureRegistry.get()
	});
	m_fbxImporter = std::make_unique<FBXImporter>();
	m_meshRegistry = std::make_unique<MeshRegistry>(uploadContext);
}

ResourceManager::~ResourceManager()
{
	CoUninitialize();
}

void ResourceManager::syncGpu(ID3D12GraphicsCommandList* cmd)
{
	m_textureRegistry->syncGpu(cmd);
}

void ResourceManager::BindDescriptorTable(ID3D12GraphicsCommandList* cmd)
{
	m_materialRegistry->BindDescriptorTable(cmd);
	m_textureRegistry->BindDescriptorTable(cmd);
}

void ResourceManager::BuildTables(ID3D12GraphicsCommandList* cmd)
{
	m_materialRegistry->BuildTable(cmd);
}

// 텍스쳐 로드 (엔진이 켜져 있는 동안 동일한 경로의 리소스는 바뀌지 않는다고 가정)
uint32_t ResourceManager::LoadTexture(const std::filesystem::path& path)
{
	return m_textureRegistry->LoadTexture(path);
}

size_t ResourceManager::AddMaterial(const Material& material)
{
	return m_materialRegistry->AddMaterial(material);
}

std::shared_ptr<Mesh> ResourceManager::LoadMeshAsset(const std::filesystem::path& path, const MeshImportOptions& options)
{
	std::string key = path.string();

	if (m_meshRegistry->HasMesh(key))
	{
		return m_meshRegistry->CreateMeshFromAsset(key);
	}

	auto importData = m_fbxImporter->LoadFile(path, options);
	if (!importData.success)
	{
		Log::Print("ResourceManager", "Failed to load mesh file: %s", key.c_str());
		return nullptr;
	}

	std::vector<uint32_t> matIndexMap;
	ProcessMaterials(path, importData.materials, matIndexMap);

	for (auto& sm : importData.submeshes)
	{
		if (sm.materialIndex < matIndexMap.size())
		{
			sm.materialIndex = matIndexMap[sm.materialIndex];
		}
		else
		{
			sm.materialIndex = 0; // Default Material
		}
	}

	auto asset = std::make_unique<MeshAsset>(path, std::move(importData.geometry), std::move(importData.submeshes), std::move(importData.materials));
	m_meshRegistry->RegisterAsset(key, std::move(asset));

	return m_meshRegistry->CreateMeshFromAsset(key);
}

const MeshAsset* ResourceManager::GetMeshAsset(const std::filesystem::path& path) const
{
	return m_meshRegistry->GetAsset(path);
}

void ResourceManager::ProcessMaterials(const std::filesystem::path& basePath, const std::vector<ImportedMaterialDesc>& srcMaterials, std::vector<uint32_t>& outMatIndices)
{
	outMatIndices.assign(srcMaterials.size(), UINT32_MAX);

	for (size_t i = 0; i < srcMaterials.size(); ++i)
	{
		const auto& desc = srcMaterials[i];

		Material matCpu;
		MaterialConstants mc{};
		mc.albedo = desc.diffuseColor;
		mc.roughness = desc.roughness;
		mc.metallic = desc.metallic;
		matCpu.SetMaterialConstants(mc);

		auto loadTex = [&](const std::wstring& relPath) {
			if (relPath.empty()) return UINT32_MAX;

			auto finalPath = ResolveTexturePath(basePath, relPath);
			return m_textureRegistry->LoadTexture(finalPath);
		};

		// 텍스처 로드 -> Material에 바인딩
		if (!desc.diffusePath.empty())   matCpu.SetDiffuseTex(loadTex(desc.diffusePath));
		if (!desc.normalPath.empty())    matCpu.SetNormalTex(loadTex(desc.normalPath));
		if (!desc.roughnessPath.empty()) matCpu.SetRoughTex(loadTex(desc.roughnessPath));
		if (!desc.metallicPath.empty())  matCpu.SetMetallicTex(loadTex(desc.metallicPath));
		if (!desc.emissivePath.empty())  matCpu.SetEmissiveTex(loadTex(desc.emissivePath));

		outMatIndices[i] = static_cast<uint32_t>(m_materialRegistry->AddMaterial(matCpu));
	}
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

	// DDS 텍스쳐 확인(Contents/Textures/...)
	auto parentDir = fbxPath.parent_path().filename();
	auto contentsRelativePath = parentDir / fileName;
	auto ddsPath = GetFullPath(AssetType::Texture, contentsRelativePath.c_str());
	if (std::filesystem::exists(ddsPath)) return ddsPath;

	// PNG 등 WIC 텍스쳐 확인(Assets/Textures/...)
	auto assetDir = fbxPath.parent_path().parent_path().parent_path();
	auto wicPath = assetDir / "Textures" / parentDir / fileName;
	if (std::filesystem::exists(wicPath)) return wicPath;

	// 못 찾았으면 원본 이름만 리턴 (TextureRegistry가 실패 처리하거나 기본 텍스처 사용)
	return fileName;
}
