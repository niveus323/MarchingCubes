#include "pch.h"
#include "CustomMeshComponent.h"
#include "Core/Geometry/Mesh/Class/DynamicMesh.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Assets/MeshRegistry.h"
#include "Core/Assets/Material/MaterialRegistry.h"
#include "Core/Assets/ResourceManager.h"

BEGIN_REFLECTION(CustomMeshComponent, MeshComponent)
END_REFLECTION()

void CustomMeshComponent::Serialize(Serializer& ar)
{
	RendererComponent::Serialize(ar);
	size_t matCount = m_materialInstnaces.size();
	ar.BeginArray("Materials", matCount);
	if (ar.IsSaving())
	{
		for (auto& mat : m_materialInstnaces)
		{
			ar.BeginObject("Material");
			ar.Serialize("Path", std::string(mat.material->GetPath()));
			ar.EndObject();
		}
	}
	else
	{
		m_materialInstnaces.resize(matCount);
		for (int i = 0; i < matCount; ++i)
		{
			ar.BeginObject("Material");
			std::string path;
			ar.Serialize("Path", path);
			m_materialInstnaces[i].material = EngineCore::GetResourceManager()->LoadMaterialAsset(path);
			ar.EndObject();
		}
	}
	ar.EndArray();
}

void CustomMeshComponent::UpdateMesh(GeometryData data, const std::vector<MeshSubmesh>& submeshes)
{
	m_data = std::move(data);
	if (!m_mesh)
	{
		m_mesh = std::make_unique<DynamicMesh>(GetName());
	}
	
	MeshRegistry* meshReg = EngineCore::GetRenderSystem()->GetMeshRegistry();
	meshReg->UpdateDynamicMesh(m_mesh, m_data, submeshes);

	SyncMaterialSlots();
}
