#include "pch.h"
#include "VertexSelector.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Utils/DXHelper.h"

VertexSelector::VertexSelector(ID3D12Device* device, UploadContext& meshUploader, const DirectX::XMFLOAT3& position, float radius) :
	m_position(position),
	m_radius(radius)
{
	static UINT s_nextID = 1;
	m_id = s_nextID++;

	MeshData meshData = CreateSphereMeshData(radius);
	m_mesh = std::make_shared<Mesh>();
	m_mesh->SetPosition(m_position);

	meshUploader.UploadMesh(*m_mesh, meshData);

	//Pick ID Constant Buffer 생성
	PickIDConstants cb{};
	cb.idColor = EncodeIDColor(m_id);
	DirectX::XMStoreFloat4(&cb.idColor, DirectX::XMLoadFloat4(&cb.idColor));
	
	const UINT pickIDConstantBufferSize = (sizeof(PickIDConstants) + 255) & ~255;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, 
		&CD3DX12_RESOURCE_DESC::Buffer(pickIDConstantBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pickIDConstantBuffer)
	));
	m_mappedPickIDCB = nullptr;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(m_pickIDConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedPickIDCB)));

	memcpy(m_mappedPickIDCB, &cb, sizeof(PickIDConstants));
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
	//cmd->SetGraphicsRootConstantBufferView(2, m_pickIDConstantBuffer->GetGPUVirtualAddress());
	XMFLOAT4 color = EncodeIDColor(m_id);
	cmd->SetGraphicsRoot32BitConstants(2, 4, &color, 0);
	m_mesh->m_buffer.Draw(cmd);
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