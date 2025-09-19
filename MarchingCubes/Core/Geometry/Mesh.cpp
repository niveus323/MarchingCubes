#include "pch.h"
#include "Mesh.h"
#include "Core/Math/PhysicsHelper.h"

MeshBuffer::MeshBuffer() : 
	m_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
	m_vertexCount(0),
	m_indexCount(0),
	m_reservedVertexCount(0),
	m_reservedIndexCount(0),
	m_mappedObjectCB(nullptr),
	m_justInitailized(false)
{
}

MeshBuffer::~MeshBuffer()
{
	if (m_mappedObjectCB)
	{
		m_objectCB->Unmap(0, nullptr);
		m_mappedObjectCB = nullptr;
	}

	m_objectCB.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
	m_vertexUploadBuffer.Reset();
	m_indexUploadBuffer.Reset();
}

void MeshBuffer::CreateBuffers(ID3D12Device* device, size_t maxVertices, size_t maxIndices)
{
	m_reservedVertexCount = UINT(maxVertices);
	m_reservedIndexCount = UINT(maxIndices);
	m_topology = D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	Initialize(device, m_reservedVertexCount * sizeof(Vertex), m_reservedIndexCount * sizeof(uint32_t));

	m_vertexCount = 0;
	m_indexCount = 0;
}

void MeshBuffer::CreateBuffers(ID3D12Device* device, const MeshData& data)
{
	m_reservedVertexCount = UINT(data.vertices.size());
	m_reservedIndexCount = UINT(data.indices.size());
	m_topology = data.topology;

	Initialize(device, m_reservedVertexCount * sizeof(Vertex), m_reservedIndexCount * sizeof(uint32_t));

	m_vertexCount = m_reservedVertexCount;
	m_indexCount = m_reservedIndexCount;
#ifdef _DEBUG
	assert(m_indexCount > 0 && "Index Count is 0 - Draw will be skipped!!!!");
#endif // _DEBUG
}

// CPU -> Upload Buffer 복사
void MeshBuffer::StageBuffers(const MeshData& data)
{
	m_vertexCount = UINT(data.vertices.size());
	m_indexCount = UINT(data.indices.size());
	const UINT vertexBufferSize = m_vertexCount * UINT(sizeof(Vertex));
	const UINT indexBufferSize = m_indexCount * UINT(sizeof(uint32_t));
	
	auto ok = [](ComPtr<ID3D12Resource> const& r, UINT need) {
		return r && r->GetDesc().Width >= need;
	};

	if (m_vertexCount && ok(m_vertexUploadBuffer, vertexBufferSize)) {
		UINT8* dst = nullptr;
		CD3DX12_RANGE range(0, 0);
		ThrowIfFailed(m_vertexUploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dst)));
		memcpy(dst, data.vertices.data(), vertexBufferSize);
		m_vertexUploadBuffer->Unmap(0, nullptr);
	}
	if (m_indexCount && ok(m_indexUploadBuffer, indexBufferSize)) {
		UINT8* dst = nullptr;
		CD3DX12_RANGE range(0, 0);
		ThrowIfFailed(m_indexUploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dst)));
		memcpy(dst, data.indices.data(), indexBufferSize);
		m_indexUploadBuffer->Unmap(0, nullptr);
	}

}

// UploadBuffer -> DefaultBuffer 복사
void MeshBuffer::CommitBuffers(ID3D12GraphicsCommandList* cmdList, const MeshData& data)
{
	m_vertexCount = UINT(data.vertices.size());
	m_indexCount = UINT(data.indices.size());

	// Vertex
	if (m_vertexCount > 0)
	{
		const UINT vertexBufferSize = m_vertexCount * UINT(sizeof(Vertex));
		
		auto before = m_justInitailized ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), before, D3D12_RESOURCE_STATE_COPY_DEST));
		
		cmdList->CopyBufferRegion(m_vertexBuffer.Get(), 0, m_vertexUploadBuffer.Get(), 0, vertexBufferSize);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	}

	// Index
	if (m_indexCount > 0)
	{
		const UINT indexBufferSize = m_indexCount * UINT(sizeof(uint32_t));

		auto before = m_justInitailized ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_INDEX_BUFFER;
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), before, D3D12_RESOURCE_STATE_COPY_DEST));
		
		cmdList->CopyBufferRegion(m_indexBuffer.Get(), 0, m_indexUploadBuffer.Get(), 0, indexBufferSize);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
	}

	m_justInitailized = false;
}

void MeshBuffer::ResizeIfNeededAndCommit(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& data)
{
	UINT vc =  UINT(data.vertices.size());
	UINT ic = UINT(data.indices.size());

	const UINT vbSize = vc * UINT(sizeof(Vertex));
	const UINT ibSize = ic * UINT(sizeof(uint32_t));

	auto needResize = [&](ComPtr<ID3D12Resource> const& r, UINT need) {
		return !r || r->GetDesc().Width < need;
	};

	if (needResize(m_vertexBuffer, vbSize) || needResize(m_indexBuffer, ibSize) ||
		needResize(m_vertexUploadBuffer, vbSize) || needResize(m_indexUploadBuffer, ibSize))
	{
		Initialize(device, std::max<UINT>(vbSize, 1), std::max<UINT>(ibSize, 1));
		m_reservedVertexCount = vc;
		m_reservedIndexCount = ic;

		// Resize 후 다시 CPU->Upload Buffer 복사
		StageBuffers(data);
	}

	CommitBuffers(cmdList, data);
}

void MeshBuffer::SetDeletionSink(std::vector<ComPtr<ID3D12Resource>>* sink)
{
	m_deletionSink = sink;
}

void MeshBuffer::CreateObjectConstantBuffer(ID3D12Device* device, const ObjectConstants& world)
{
	static const UINT objectConstantBufferSize = AlignUp(sizeof(ObjectConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, 
		&CD3DX12_RESOURCE_DESC::Buffer(objectConstantBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, 
		nullptr, 
		IID_PPV_ARGS(&m_objectCB)
	));
	NAME_D3D12_OBJECT(m_objectCB);

	// Map & CreateUploadBuffer Constant Buffer
	ThrowIfFailed(m_objectCB->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedObjectCB)));
	memcpy(m_mappedObjectCB, &world, sizeof(world));
}

void MeshBuffer::UpdateObjectConstantBuffer(const ObjectConstants& cb)
{	
	memcpy(m_mappedObjectCB, &cb, sizeof(cb));
}

void MeshBuffer::BindObjectConstants(ID3D12GraphicsCommandList* cmdList) const
{
	cmdList->SetGraphicsRootConstantBufferView(1, m_objectCB->GetGPUVirtualAddress());
}

void MeshBuffer::Draw(ID3D12GraphicsCommandList* cmdList) const
{
	if (m_indexCount == 0) return;

	cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	cmdList->IASetIndexBuffer(&m_indexBufferView);
	cmdList->IASetPrimitiveTopology(m_topology);
	cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}

void MeshBuffer::Initialize(ID3D12Device* device, UINT vertexBufferSize, UINT indexBufferSize)
{
	// Push_back Old Buffer to DeletionSink
	if (m_deletionSink)
	{
		if (m_vertexBuffer) m_deletionSink->push_back(std::move(m_vertexBuffer));
		if (m_indexBuffer) m_deletionSink->push_back(std::move(m_indexBuffer));
		if (m_vertexUploadBuffer) m_deletionSink->push_back(std::move(m_vertexUploadBuffer));
		if (m_indexUploadBuffer) m_deletionSink->push_back(std::move(m_indexUploadBuffer));
	}

	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

	// Create Heap
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
			D3D12_HEAP_FLAG_NONE, 
			&vertexBufferDesc, 
			D3D12_RESOURCE_STATE_COMMON, 
			nullptr, 
			IID_PPV_ARGS(&m_vertexBuffer)
		));
		NAME_D3D12_OBJECT(m_vertexBuffer);
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
			D3D12_HEAP_FLAG_NONE, 
			&indexBufferDesc, 
			D3D12_RESOURCE_STATE_COMMON,
			nullptr, 
			IID_PPV_ARGS(&m_indexBuffer)
		));
		NAME_D3D12_OBJECT(m_indexBuffer);
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), 
			D3D12_HEAP_FLAG_NONE, 
			&vertexBufferDesc, 
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			nullptr, 
			IID_PPV_ARGS(&m_vertexUploadBuffer)
		));
		NAME_D3D12_OBJECT(m_vertexUploadBuffer);
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), 
			D3D12_HEAP_FLAG_NONE, 
			&indexBufferDesc, 
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, 
			IID_PPV_ARGS(&m_indexUploadBuffer)
		));
		NAME_D3D12_OBJECT(m_indexUploadBuffer);
	}

	// CreateUploadBuffer BufferView
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = indexBufferSize;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;	
	
	m_justInitailized = true;
}

void Mesh::Draw(ID3D12GraphicsCommandList* cmdList) const
{
	SetConstantsBuffers(cmdList);
	m_buffer.Draw(cmdList);
}

void Mesh::SetConstantsBuffers(ID3D12GraphicsCommandList* cmdList) const
{
	m_buffer.BindObjectConstants(cmdList);
	
	if (m_material)
	{
		m_material->BindConstant(cmdList);
	}
}

void Mesh::Initialize(ID3D12Device* device, const MeshData& meshData)
{
	m_data = meshData;
	if (meshData.vertices.empty() || meshData.indices.empty())
	{
		m_data.vertices.reserve(meshData.vertices.capacity());
		m_data.indices.reserve(meshData.indices.capacity());
	}
	Upload(device);
	m_buffer.SetTopology(m_data.topology);

	FillObjectConstants();
	m_buffer.CreateObjectConstantBuffer(device, m_objectCB);

	// MaterialCB는 App 클래스에서 공유받아 사용하기 때문에 Create가 완료된 상태를 사용.
}

void Mesh::Update(float deltaTime)
{
	UpdateConstants();
}

void Mesh::Upload(ID3D12Device* device)
{
	if (m_data.vertices.empty() || m_data.indices.empty())
	{
		m_buffer.CreateBuffers(device, m_data.vertices.capacity(), m_data.indices.capacity());
	}
	else
	{
		m_buffer.CreateBuffers(device, m_data);
	}
}

void Mesh::UpdateConstants()
{
	FillObjectConstants();
	m_buffer.UpdateObjectConstantBuffer(m_objectCB);
}

void Mesh::StageBuffers()
{
	m_buffer.StageBuffers(m_data);
	BuildTriBounds();
}

void Mesh::CommitBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	m_buffer.ResizeIfNeededAndCommit(device, cmdList, m_data);
}

void Mesh::UpdateData(const MeshData& meshData)
{
	m_data = meshData;
	StageBuffers();
}

DirectX::XMMATRIX Mesh::GetWorldMatrix() const
{
	XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX R = XMMatrixRotationQuaternion(ToQuatFromEuler(m_rotation));
	XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	return S * R * T;
}

DirectX::XMMATRIX Mesh::GetWorldInvMatrix() const
{
	return XMMatrixInverse(nullptr, GetWorldMatrix());
}

void Mesh::SetColor(const DirectX::XMFLOAT4& color)
{
	for (auto& v : m_data.vertices)
	{
		v.color = color;
	}
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

void Mesh::BuildTriBounds()
{
	// AABB BroadPhase를 위한 Bound 세팅
	const auto& vertices = m_data.vertices;
	const auto& indices = m_data.indices;
	size_t triCount = indices.size() / 3;
	m_triBounds.clear();
	m_triBounds.resize(triCount);

	for (size_t t = 0; t < triCount; ++t)
	{
		XMFLOAT3 points[3]
		{
			vertices[indices[3 * t + 0]].pos,
			vertices[indices[3 * t + 1]].pos,
			vertices[indices[3 * t + 2]].pos
		};

		BoundingBox::CreateFromPoints(m_triBounds[t], 3, points, sizeof(XMFLOAT3));
	}
}

void Mesh::FillObjectConstants()
{
	using namespace DirectX;

	XMMATRIX worldTrans = XMMatrixTranspose(GetWorldMatrix());
	XMMATRIX worldInvTrans = XMMatrixTranspose(GetWorldInvMatrix());

	XMStoreFloat4x4(&m_objectCB.worldMatrix, worldTrans);
	XMStoreFloat4x4(&m_objectCB.worldInvMatrix, worldInvTrans);
}
