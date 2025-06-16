#include "pch.h"
#include "Mesh.h"
#include "Core/Utils/DXHelper.h"

void MeshBuffer::CreateBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& data)
{
	const UINT vertexBufferSize = UINT(data.vertices.size() * sizeof(Vertex));
	const UINT indexBufferSize = UINT(data.indices.size() * sizeof(uint32_t));
	m_topology = data.topology;

	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

	// Create Heap
	{
		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_vertexBuffer)));
		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &indexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_indexBuffer)));
		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexUploadBuffer)));
		ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &indexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_indexUploadBuffer)));
	}

	// Copy Mesh Data To VertexBuffer
	{
		// CPU의 vertex 데이터를 GPU의 Upload Heap에 복사
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_vertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, data.vertices.data(), vertexBufferSize);
		m_vertexUploadBuffer->Unmap(0, nullptr);

		// Copy Upload Heap To Default Heap 
		D3D12_SUBRESOURCE_DATA vertexData = { data.vertices.data(), vertexBufferSize, vertexBufferSize};
		UpdateSubresources(cmdList, m_vertexBuffer.Get(), m_vertexUploadBuffer.Get(), 0, 0, 1, &vertexData);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	}

	// Copy Index Data To IndexBuffer
	{
		// CPU의 index 데이터를 GPU의 Upload Heap에 복사
		UINT8* pIndexDataBegin;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_indexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
		memcpy(pIndexDataBegin, data.indices.data(), indexBufferSize);
		m_indexUploadBuffer->Unmap(0, nullptr);

		// Copy Upload Heap To Default Heap
		D3D12_SUBRESOURCE_DATA indexData = { data.indices.data(), indexBufferSize, indexBufferSize};
		UpdateSubresources(cmdList, m_indexBuffer.Get(), m_indexUploadBuffer.Get(), 0, 0, 1, &indexData);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
	}

	// Initialize BufferView
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = indexBufferSize;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	m_indexCount = UINT(data.indices.size());

#ifdef _DEBUG
	assert(m_indexCount > 0 && "Index Count is 0 - Draw will be skipped!!!!");
#endif // _DEBUG

}

void MeshBuffer::CreateObjectConstantBuffer(ID3D12Device* device, const ObjectConstants& world)
{
	const UINT objectConstantBufferSize = (sizeof(world) + 255) & ~255;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(objectConstantBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_objectConstantsBuffer)
	));

	// Map & Initialize Constant Buffer
	m_mappedObjectCB = nullptr;
	CD3DX12_RANGE readRAnge(0, 0);
	ThrowIfFailed(m_objectConstantsBuffer->Map(0, &readRAnge, reinterpret_cast<void**>(&m_mappedObjectCB)));

	memcpy(m_mappedObjectCB, &world, sizeof(ObjectConstants));
}

void MeshBuffer::UpdateObjectConstantBuffer(const DirectX::XMFLOAT3& position, const DirectX::XMVECTOR& quat, const DirectX::XMFLOAT3& scale)
{
	using namespace DirectX;
	
#ifdef _DEBUG
	assert(m_mappedObjectCB && "Mesh ObjectCB not mapped!!!!");
#endif // _DEBUG

	XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
	XMMATRIX R = XMMatrixRotationQuaternion(quat);
	XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
	
	ObjectConstants cb{};
	XMMATRIX world = S * R * T;
	XMStoreFloat4x4(&cb.worldMatrix, XMMatrixTranspose(world));
	memcpy(m_mappedObjectCB, &cb, sizeof(cb));
}

void MeshBuffer::Draw(ID3D12GraphicsCommandList* cmd) const
{
	cmd->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	cmd->IASetIndexBuffer(&m_indexBufferView);
	cmd->IASetPrimitiveTopology(m_topology);
	cmd->SetGraphicsRootConstantBufferView(1, m_objectConstantsBuffer->GetGPUVirtualAddress());
	cmd->DrawIndexedInstanced(GetIndexCount(), 1, 0, 0, 0);
}

void Mesh::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& meshData)
{
	m_buffer.CreateBuffers(device, cmdList, meshData);
	ObjectConstants cb{};
	XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX R = XMMatrixRotationQuaternion(ToQuatFromEuler(m_rotation));
	XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	XMMATRIX world = S * R * T;
	XMStoreFloat4x4(&cb.worldMatrix, XMMatrixTranspose(world));
	m_buffer.CreateObjectConstantBuffer(device, cb);
}

void Mesh::Update()
{
	m_buffer.UpdateObjectConstantBuffer(m_position, ToQuatFromEuler(m_rotation), m_scale);
}

void Mesh::SetPosition(const DirectX::XMFLOAT3& pos)
{
	m_position = pos;
}

void Mesh::SetRotation(const DirectX::XMFLOAT3& quat)
{
	m_rotation = quat;
}

void Mesh::SetScale(const DirectX::XMFLOAT3& scale)
{
	m_scale = scale;
}

void Mesh::Move(const DirectX::XMFLOAT3& delta)
{
	m_position.x += delta.x;
	m_position.y += delta.y;
	m_position.z += delta.z;
}

void Mesh::Rotate(const DirectX::XMVECTOR& deltaQuat)
{
	using namespace DirectX;
	XMVECTOR current = ToQuatFromEuler(m_rotation);
	XMVECTOR result = XMQuaternionNormalize(XMQuaternionMultiply(current, deltaQuat));
	m_rotation = ToEulerFromQuat(result);	
}

void Mesh::Scale(const DirectX::XMFLOAT4& scaleFactor)
{
	m_scale.x *= scaleFactor.x;
	m_scale.y *= scaleFactor.y;
	m_scale.z *= scaleFactor.z;
}

