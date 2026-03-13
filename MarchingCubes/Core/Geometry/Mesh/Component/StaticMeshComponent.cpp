#include "pch.h"
#include "StaticMeshComponent.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Assets/MeshRegistry.h"
#include "Core/Assets/Material/MaterialRegistry.h"
#include "Core/Geometry/Mesh/Class/Mesh.h"
#include "Core/Assets/MeshAsset.h"
#include "Core/Assets/Material/MaterialAsset.h"

BEGIN_REFLECTION(StaticMeshComponent, MeshComponent)
	REFLECT_PROPERTY_ASSET(m_path, MeshAsset, "Mesh")
	//REFLECT_PROPERTY_FN("Mesh", EPropertyType::Asset, MeshAsset, )
END_REFLECTION()

void StaticMeshComponent::Serialize(Serializer& ar)
{
	MeshComponent::Serialize(ar);
	ar.Serialize("MeshPath", m_path);
	if (!ar.IsSaving())
	{
		if (!m_path.empty()) SetMeshByPath(m_path);
	}

	size_t matCount = m_materialInstnaces.size();
	ar.BeginArray("Materials", matCount);
	if (ar.IsSaving())
	{
		for (const auto& materialAsset : m_materialInstnaces)
		{
			std::string tempPath = materialAsset.m_material->GetPath().data();
			ar.BeginObject("MaterialSlot");
			ar.Serialize("Path", tempPath);
			ar.EndObject();
		}
	}
	else
	{
		m_materialInstnaces.resize(matCount);
		for (int i = 0; i < matCount; ++i)
		{
			ar.BeginObject("MaterialSlot");
			std::string path;
			ar.Serialize("Path", path);
			SetMaterialByPath(i, path);
			ar.EndObject();
		}
	}
	ar.EndArray();
}

void StaticMeshComponent::SetMeshByPath(const std::string& path)
{
	if (m_meshAsset && path == m_meshAsset->GetSourcePath()) return;
	
	m_path = path;
	if (m_path.empty())
	{
		m_meshAsset.reset();
		m_mesh.reset();
		m_materialInstnaces.clear();
		return;
	}

	auto resourceManager = EngineCore::GetResourceManager();
	if (m_meshAsset = resourceManager->LoadMeshAsset(m_path, MeshImportOptions{}))
	{
		MeshRegistry* meshReg = EngineCore::GetRenderSystem()->GetMeshRegistry();
		MaterialRegistry* matReg = EngineCore::GetRenderSystem()->GetMaterialRegistry();

		m_mesh = meshReg->CreateStaticMesh(m_path, m_meshAsset->GetGeometry(), m_meshAsset->GetSubmesh());
		const auto& matAssets = m_meshAsset->GetMaterialAssets();
		m_materialInstnaces.resize(matAssets.size());
		for (int i=0; i< matAssets.size(); ++i)
		{
			m_materialInstnaces[i] = MaterialInstance{
				.m_material = matAssets[i]
			};
		}
		SyncMaterialSlots();
	}
	else
	{
		Log::Print("StaticMeshComponent", "Failed to load mesh: %s", path.c_str());
	}
}