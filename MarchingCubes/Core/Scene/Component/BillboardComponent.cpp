#include "pch.h"
#include "BillboardComponent.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Assets/MeshRegistry.h"
#include "Core/Assets/Material/MaterialRegistry.h"
#include "Core/Assets/MeshAsset.h"
#include "Core/Assets/TextureAsset.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Geometry/Mesh/Class/StaticMesh.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(BillboardComponent, RendererComponent)
	REFLECT_PROPERTY(m_size, EPropertyType::Vector2, "Size")
END_REFLECTION()

void BillboardComponent::Init()
{
	RendererComponent::Init();
	if (auto asset = EngineCore::GetResourceManager()->GetMeshAsset("@BillboardQuad"))
	{
		auto registry = EngineCore::GetRenderSystem()->GetMeshRegistry();
		m_quadMesh = registry->CreateStaticMesh(asset);
	}
	// 디폴트 아이콘 세팅
	auto iconMat = EngineCore::GetResourceManager()->LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/EditorBillboard.json"));
	SetMaterial(iconMat);
}

void BillboardComponent::Destroy()
{
	RendererComponent::Destroy();
}

void BillboardComponent::Submit()
{
	// GameObject를 통해 동적으로 추가된 Billboard는 Material이 nullptr이다.
	if (!IsActive() || !m_iconMat.m_material || !m_quadMesh) return;

	if (auto meshComp = GetOwner()->GetComponent<MeshComponent>())
	{
		// 유효한 메쉬가 있다면 빌보드는 렌더링하지 않음
		Mesh* mesh = meshComp->GetMesh();
		if (mesh) return;
	}

	auto renderSystem = EngineCore::GetRenderSystem();
	auto uploadContext = EngineCore::GetUploadContext();
	if (!renderSystem || !uploadContext) return;
	
	uint32_t materialGPUIndex = UINT32_MAX;
	if (!m_iconMat.m_overrides.empty())
	{
		std::string matInstanceKey = std::format("MatInst_Billboard_{}", m_iconIdentifier);
		if (m_iconMat.m_bDirty)
		{
			materialGPUIndex = renderSystem->GetMaterialRegistry()->RegisterMaterialInstance(m_iconMat, matInstanceKey);
			m_iconMat.m_bDirty = false;
		}
		else
		{
			materialGPUIndex = renderSystem->GetMaterialRegistry()->FindMaterialHandle(matInstanceKey);
		}
	}
	else 
	{
		materialGPUIndex = renderSystem->GetMaterialRegistry()->RegisterMaterialAsset(m_iconMat.m_material);
	}
	
	ObjectConstants objConsts{};
	objConsts.materialIndex = materialGPUIndex;
	XMMATRIX worldMatrix = GetWorldMatrix();
	XMStoreFloat4x4(&objConsts.worldMatrix, XMMatrixTranspose(worldMatrix));
	XMStoreFloat4x4(&objConsts.worldInvMatrix, XMMatrixInverse(nullptr, worldMatrix));
	uploadContext->UploadConstants(&objConsts, sizeof(ObjectConstants), m_objectCB);

	const auto& subMesh = m_quadMesh->GetSubmeshes()[0];

	RenderItem* item = renderSystem->AllocateRenderItem();
	if (!item) return;

	item->meshBuffer = m_quadMesh->GetGPUBuffer();
	item->topology = m_quadMesh->GetTopology();
	item->indexCount = subMesh.indexCount;
	item->indexOffset = subMesh.indexOffset;
	item->baseVertexLocation = subMesh.baseVertexLocation;
	item->materialIndex = materialGPUIndex;
	item->objectID = GetOwner()->GetObjectID();
	item->debugName = m_name;
	item->resourceBindings.push_back(ShaderBinding{
		.type = EBindingType::CBV,
		.rootParamKey = "ObjectBuffer",
		.gpuAddress = m_objectCB.gpuVA
	});
	renderSystem->SubmitRenderItem(item, m_iconMat.GetPSO());

	// OverlayPass 추가 렌더링
	for (auto& pass : m_overlayPasses)
	{
		if (!pass.bActive) continue;
		RenderItem* overlayItem = renderSystem->AllocateRenderItem();
		if (!overlayItem) continue;
		
		*overlayItem = *item; //복사
		overlayItem->debugName = m_name + "_" + pass.name;
		overlayItem->resourceBindings.reserve(overlayItem->resourceBindings.size() + pass.resourceBindings.size());
		overlayItem->resourceBindings.insert(overlayItem->resourceBindings.end(), pass.resourceBindings.begin(), pass.resourceBindings.end());

		renderSystem->SubmitRenderItem(overlayItem, pass.psoName);
	}
}

void BillboardComponent::Serialize(Serializer& ar)
{
	RendererComponent::Serialize(ar);
	ar.Serialize("Size", m_size);
	std::string materialPath = m_iconMat.m_material->GetPath().data();
	ar.Serialize("Material", materialPath);
	if(!ar.IsSaving())
	{
		m_iconMat.m_material = EngineCore::GetResourceManager()->LoadMaterialAsset(materialPath);
	}
}

void BillboardComponent::SetIcon(std::shared_ptr<TextureAsset> textureAsset, int priority)
{
	if (priority < m_priority) return;

	m_iconMat.SetParameter("DiffuseTexture", textureAsset);
	m_priority = priority;
	m_iconIdentifier = std::filesystem::path(textureAsset->GetSourcePath()).filename().string();
}

DirectX::XMMATRIX BillboardComponent::GetWorldMatrix() const
{
	XMMATRIX worldMat = m_transformCache->GetWorldMatrix();
	DirectX::XMMATRIX sizeMat = DirectX::XMMatrixScaling(m_size.x, m_size.y, 1.0f);

	return DirectX::XMMatrixMultiply(sizeMat, worldMat);
}
