#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/Assets/MeshAsset.h" // ImportedMaterialDesc 등 정의 포함
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

// FBX SDK 전방 선언 (헤더 오염 방지)
namespace fbxsdk {
	class FbxManager;
	class FbxScene;
	class FbxNode;
	class FbxMesh;
	class FbxSurfaceMaterial;
}

struct MeshImportOptions
{
	float uniformScale = 1.0f;    // ex) 0.01f: cm → m
	bool  applyUnitConversion = true;
};

class FBXImporter
{
public:
	struct ImportSceneData
	{
		GeometryData geometry;
		std::vector<MeshSubmesh> submeshes;
		std::vector<ImportedMaterialDesc> materials;
		bool success = false;
	};

	FBXImporter();
	~FBXImporter();

	ImportSceneData LoadFile(const std::filesystem::path& path, const MeshImportOptions& options);

private:
	void InitializeSDK();
	void CleanupSDK();

	void ProcessScene(fbxsdk::FbxScene* scene, ImportSceneData& outData, const MeshImportOptions& options);
	void CollectMeshes(fbxsdk::FbxNode* node, std::vector<fbxsdk::FbxMesh*>& outMeshes);
	ImportedMaterialDesc ParseMaterial(const fbxsdk::FbxSurfaceMaterial* fbxMat);

	void AppendFbxMeshToGeometry(fbxsdk::FbxMesh* fbxMesh, const std::unordered_map<fbxsdk::FbxSurfaceMaterial*, uint32_t>& matPtrToGlobalIndex, GeometryData& outGeometry, std::vector<MeshSubmesh>* outSubmeshes);
	void ApplyUniformScale(GeometryData& data, float s);
	int GetLocalMaterialIndex(fbxsdk::FbxMesh* mesh, int polyIndex);
	void PrintLayerInfo(fbxsdk::FbxMesh* pMeshNode);

private:
	fbxsdk::FbxManager* m_manager = nullptr;
};