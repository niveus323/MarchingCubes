#include "pch.h"
#include "VertexSelector.h"
#include "Core/Geometry/MeshGenerator.h"

VertexSelector::VertexSelector(UploadContext& meshUploader, const DirectX::XMFLOAT3& position, float radius) :
	m_position(position),
	m_radius(radius)
{
	static UINT s_nextID = 1;
	m_id = s_nextID++;

	MeshData meshData = CreateSphereMeshData(radius);
	m_mesh = std::make_shared<Mesh>();
	m_mesh->SetPosition(m_position);

	meshUploader.UploadMesh(*m_mesh, meshData);
}

void VertexSelector::SetHovered(bool bHovered)
{
	if (m_hovered != bHovered)
	{
		m_hovered = bHovered;
		//m_mesh.SetColor(bHovered ? DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) : DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
	}

}

void VertexSelector::RenderForPicking(ID3D12GraphicsCommandList* cmd) const
{
	m_mesh->m_buffer.Draw(cmd);
}