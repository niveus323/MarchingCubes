#include "pch.h"
#include "MeshComponent.h"
#include "Core/Geometry/Mesh/Class/Mesh.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Assets/Material/MaterialRegistry.h"
#include "Core/Assets/ResourceManager.h"

BEGIN_REFLECTION(MeshComponent, RendererComponent)
    REFLECT_PROPERTY_ASSET_ARRAY_FN("Materials", Material, GetMaterialSlotCount, GetMaterialPath, SetMaterialByPath)
END_REFLECTION()

void MeshComponent::Init()
{
    RendererComponent::Init();
    SetMaterialByPath(0, GetFullPath(AssetType::Default, L"Material/DefaultFilled.json").string());
}

void MeshComponent::Submit()
{
    Mesh* mesh = GetMesh();
    if (!mesh)
    {
        Log::Print("MeshComponent", "Sumitted Invalid Mesh");
        return;
    }
    
    if (m_materialInstnaces.size() != mesh->GetSubmeshCount()) SyncMaterialSlots();

    auto renderSystem = EngineCore::GetRenderSystem();
    auto uploadContext = EngineCore::GetUploadContext();
    assert(renderSystem && uploadContext);

    // Material Asset -> GPU Index로 변환
    std::vector<uint32_t> materialIndices(m_materialInstnaces.size());
    for (int i = 0; i < m_materialInstnaces.size(); ++i)
    {
        auto& matInst = m_materialInstnaces[i];
        uint32_t materialGPUIndex = UINT32_MAX;
        if (!matInst.m_overrides.empty())
        {
            std::string matInstanceKey = std::format("MatInst_{}[{}]", GetMesh()->GetDebugName(), i);
            if (matInst.m_bDirty)
            {
                // 수정 발생 시 Registry에 변경 적용
                materialGPUIndex = renderSystem->GetMaterialRegistry()->RegisterMaterialInstance(matInst, matInstanceKey);
                matInst.m_bDirty = false;
            }
            else
            {
                //수정이 없었을 경우 캐시에서 받아온다
                materialGPUIndex = renderSystem->GetMaterialRegistry()->FindMaterialHandle(matInstanceKey);
            }
        }
        else
        {
            // 기존 MaterialAsset을 수정하지 않는 Instance라면 이미 업로드된 Material을 사용
            materialGPUIndex = renderSystem->GetMaterialRegistry()->RegisterMaterialAsset(matInst.m_material);
        }

        
        materialIndices[i] = materialGPUIndex;
    }

    // CBV 업로드
    const auto& submeshes = mesh->GetSubmeshes();
    if (auto uploadContext = EngineCore::GetUploadContext())
    {
        XMMATRIX worldMatrix = m_transformCache->GetWorldMatrix();
        m_objectCBList.resize(submeshes.size());
        for (size_t i = 0; i < submeshes.size(); ++i)
        {
            ObjectConstants objConsts{};
            objConsts.materialIndex = materialIndices[submeshes[i].materialslot];
            // row-major -> column-major 변환
            XMStoreFloat4x4(&objConsts.worldMatrix, XMMatrixTranspose(worldMatrix));
            XMStoreFloat4x4(&objConsts.worldInvMatrix, XMMatrixInverse(nullptr, worldMatrix));

            uploadContext->UploadConstants(&objConsts, sizeof(ObjectConstants), m_objectCBList[i]);
        }
    }

    GeometryBuffer* gpuBuffer = mesh->GetGPUBuffer();
    D3D12_PRIMITIVE_TOPOLOGY topology = mesh->GetTopology(); // PSO가 Topology를 사용하고 있어야 함
    for (size_t i = 0; i < submeshes.size(); ++i)
    {
        const auto& subMesh = submeshes[i];
        RenderItem* item = renderSystem->AllocateRenderItem();
        if (!item) return;

        item->meshBuffer = gpuBuffer;
        item->topology = topology;
        item->indexCount = subMesh.indexCount;
        item->indexOffset = subMesh.indexOffset;
        item->baseVertexLocation = subMesh.baseVertexLocation;
        item->materialIndex = materialIndices[subMesh.materialslot];
        item->objectID = GetOwner()->GetObjectID();
        item->debugName = m_name;
        item->resourceBindings.push_back(ShaderBinding{
            .type = EBindingType::CBV,
            .rootParamKey = "ObjectBuffer",
            .gpuAddress = m_objectCBList[i].gpuVA
        });
        renderSystem->SubmitRenderItem(item, m_materialInstnaces[subMesh.materialslot].GetPSO());

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
}

void MeshComponent::Serialize(Serializer& ar)
{
    RendererComponent::Serialize(ar);
}

void MeshComponent::SetPSO(int slot, std::string_view psoName)
{
    if (slot >= 0 && slot < m_materialInstnaces.size())
        m_materialInstnaces[slot].m_psoName = psoName.data();
}

void MeshComponent::SetPSO(std::string_view psoName)
{
    for (int i = 0; i < m_materialInstnaces.size(); ++i)
        SetPSO(i, psoName);
}

void MeshComponent::SyncMaterialSlots()
{
    Mesh* mesh = GetMesh();
    if (!mesh)
    {
        m_materialInstnaces.clear();
        m_objectCBList.clear(); // 상수 버퍼도 정리
        return;
    }

    const size_t submeshCount = mesh->GetSubmeshCount();
    const auto& submeshes = mesh->GetSubmeshes();

    if (m_materialInstnaces.size() != submeshCount)
    {
        m_materialInstnaces.resize(submeshCount);
        m_objectCBList.resize(submeshCount);
    }

    for (size_t i = 1; i < submeshCount; ++i)
    {
        if (!m_materialInstnaces[i].m_material)
        {
            m_materialInstnaces[i] = m_materialInstnaces[0];
        }
    }
}

void MeshComponent::SetMaterialByPath(int slot, const std::string& matPath)
{
    if (!matPath.empty())
    {
        auto asset = EngineCore::GetResourceManager()->LoadMaterialAsset(matPath);
        SetMaterial(slot, asset);
    }
}

void MeshComponent::SetMaterial(int slot, std::shared_ptr<MaterialAsset> matAsset)
{
    if (slot < 0 || slot >= m_materialInstnaces.size()) return;
    if (m_materialInstnaces.size() <= slot) m_materialInstnaces.resize(slot + 1);
    
    if (!matAsset)
    {
        m_materialInstnaces[slot].m_material = m_materialInstnaces[0].m_material;
        m_materialInstnaces[slot].m_bDirty = true;
        return;
    }
    m_materialInstnaces[slot].m_material = matAsset;
    m_materialInstnaces[slot].m_overrides.clear();
    m_materialInstnaces[slot].m_bDirty = true;
}
