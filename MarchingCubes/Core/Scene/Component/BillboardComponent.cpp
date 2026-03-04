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
	XMMATRIX worldMatrix = GetWorldMatrix(GetScene()->GetMainCamera()); // 현재 렌더링을 하는 씬의 MainCamera를 기준으로 WorldMatrix 보정
	XMStoreFloat4x4(&objConsts.worldMatrix, XMMatrixTranspose(worldMatrix));
	XMStoreFloat4x4(&objConsts.worldInvMatrix, XMMatrixInverse(nullptr, worldMatrix));
	uploadContext->UploadConstants(&objConsts, sizeof(ObjectConstants), m_objectCB);

	const auto& subMesh = m_quadMesh->GetSubmeshes()[0];
	RenderItem item{
		.meshBuffer = m_quadMesh->GetGPUBuffer(),
		.topology = m_quadMesh->GetTopology(),
		.indexCount = subMesh.indexCount,
		.indexOffset = subMesh.indexOffset,
		.baseVertexLocation = subMesh.baseVertexLocation,
		.materialIndex = materialGPUIndex,
		.debugName = m_name
	};

	item.resourceBindings.push_back(ShaderBinding{
		.type = EBindingType::CBV,
		.rootParameterIndex = 1,
		.gpuAddress = m_objectCB.gpuVA
	});
	
	renderSystem->SubmitRenderItem(item, m_iconMat.GetPSO());
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

DirectX::BoundingBox BillboardComponent::GetBoundingBox() const
{
	DirectX::BoundingBox box;
	if (auto transform = GetTransformComp())
	{
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 extents = { m_size.x * 0.5f, m_size.y * 0.5f, 0.1f }; // NOTE : z값에 의해 피킹이 잘 이루어지지 않을 경우 수정
		box = DirectX::BoundingBox(pos, extents);
	}
	return box;
}

DirectX::XMMATRIX BillboardComponent::GetWorldMatrix(CameraComponent* camera)
{
	XMMATRIX worldMat = m_transformCache->GetWorldMatrix();
	
	XMVECTOR position, rotQuat, scale;
	XMMatrixDecompose(&scale, &rotQuat, &position, worldMat);

	auto cameraTransformComp = camera->GetTransformComp();
	XMVECTOR camPos = XMLoadFloat3(&cameraTransformComp->GetWorldPosition());
	float constantScreenSizeFactor = 5.0f;
	float finalIconScale = constantScreenSizeFactor;

	XMMATRIX scaleMat = XMMatrixScaling(finalIconScale, finalIconScale, finalIconScale);
	XMMATRIX invView = XMMatrixInverse(nullptr, camera->GetViewMatrix());
	invView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	return scaleMat * invView * XMMatrixTranslationFromVector(position);
}
