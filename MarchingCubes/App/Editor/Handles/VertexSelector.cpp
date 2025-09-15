#include "pch.h"
#include "VertexSelector.h"
#include "Core/Geometry/MeshGenerator.h"

VertexSelector::VertexSelector(ID3D12Device* device, UploadContext& meshUploader, const DirectX::XMFLOAT3& position, int id, float radius, const DirectX::XMFLOAT4& defaultColor, const DirectX::XMFLOAT4& selectedColor) :
	m_position(position),
	m_id(id),
	m_radius(radius),
	m_defaultcolor(defaultColor),
	m_selectedColor(selectedColor)
{
	MeshData meshData = MeshGenerator::CreateSphereMeshData(radius, defaultColor);
	m_mesh = std::make_unique<Mesh>();
	m_mesh->SetPosition(m_position);

	m_renderItem = meshUploader.UploadDynamicMesh(*m_mesh, meshData);
}

VertexSelector::~VertexSelector()
{
	m_mesh.reset();
}

void VertexSelector::SetHovered(bool bHovered)
{
	if (m_hovered != bHovered)
	{
		m_hovered = bHovered;
		//m_mesh.SetColor(bHovered ? DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) : DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
	}

}

void VertexSelector::SetSelected(bool bSelected)
{
	m_selected = bSelected;
	m_mesh->SetColor(m_selected ? m_selectedColor : m_defaultcolor);
}

DirectX::XMFLOAT4 VertexSelector::EncodeIDColor(uint32_t id) const
{
	//시각화를 위해 A값은 1로 고정 -> 24비트로만 id를 사용.
	uint32_t id24 = id & 0xFFFFFF;
	static const uint32_t KNUTH_HASH = 2654435761u;
	uint32_t scrambled = id * KNUTH_HASH;
	
	uint8_t r = uint8_t((scrambled >> 16) & 0xFF);
	uint8_t g = uint8_t((scrambled >> 8) & 0xFF);
	uint8_t b = uint8_t((scrambled) & 0xFF);
	
	constexpr float inv255 = 1.0f / 255.0f;
	
	return XMFLOAT4(float(r) * inv255, float(g) * inv255, float(b) * inv255, 1.0f);
}