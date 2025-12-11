#include "pch.h"
#include "MeshComponent.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Scene/BaseScene.h"
#include "Core/Scene/SceneObject.h"
#include "Core/Scene/Component/TransformComponent.h"
#include "Core/Rendering/RenderSystem.h"

MeshComponent::MeshComponent(SceneObject* owner, Mesh* mesh, std::string_view psoName) :
    MeshComponent(owner)
{
    SetMesh(mesh);
    SetPSO(psoName);
}

MeshComponent::MeshComponent(SceneObject* owner, Mesh* mesh, const std::vector<std::string_view>& psoNames) : 
    MeshComponent(owner)
{
    SetMesh(mesh);
    for (int i = 0; i < psoNames.size(); ++i)
    {
        SetPSO(i, psoNames[i]);
    }
}

void MeshComponent::SetMesh(Mesh* mesh)
{
    m_mesh = mesh;
    if (m_mesh)
    {
        const auto& submeshes = m_mesh->GetSubmeshes();
        m_materials.resize(submeshes.size());

        for (size_t i = 0; i < submeshes.size(); ++i)
        {
            m_materials[i].index = submeshes[i].materialIndex;
            m_materials[i].psoName = "Filled";
        }
    }
}

void MeshComponent::SetMaterial(int slot, uint32_t materialHandle)
{
    if (slot >= 0 && slot < m_materials.size())
        m_materials[slot].index = materialHandle;
}

void MeshComponent::SetPSO(int slot, std::string_view psoName)
{
    if (slot >= 0 && slot < m_materials.size())
        m_materials[slot].psoName = psoName.data();
}
void MeshComponent::SetPSO(std::string_view psoName)
{
    for (int i = 0; i < m_materials.size(); ++i) 
        SetPSO(i, psoName);
}

void MeshComponent::Submit()
{
    if (!m_mesh) 
    {
        Log::Print("MeshComponent", "Sumitted Invalid Mesh");
        return;
    }

    auto renderSystem = GetScene()->GetRenderSystem();
    if (!renderSystem) 
    {
        Log::Print("MeshComponent", "Invalid RenderSystem");
        return;
    }

    const auto& submeshes = m_mesh->GetSubmeshes();
    GeometryBuffer* gpuBuffer = m_mesh->GetGPUBuffer();
    for (size_t i = 0; i < submeshes.size(); ++i)
    {
        const auto& mesh = submeshes[i];
        RenderItem item{
            .meshBuffer = gpuBuffer,
            .topology = m_mesh->GetCPUData()->topology,
            .indexCount = mesh.indexCount,
            .indexOffset = mesh.indexOffset,
            .baseVertexLocation = mesh.baseVertexLocation,
            .worldMatrix = m_owner->GetTransform(),
            .materialIndex = mesh.materialIndex,
        };
        renderSystem->SubmitRenderItem(item, m_materials[i].psoName);
    }
}

