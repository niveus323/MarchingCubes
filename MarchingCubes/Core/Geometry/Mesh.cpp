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

int MeshBuffer::FindOrCreateFallbackSlot(ID3D12Device* device, ID3D12Fence* graphicsFence, UINT64 neededSize) {
	// try find free slot whose fence <= completed
	UINT64 completed = graphicsFence->GetCompletedValue();
	for (int i = 0; i < kFallbackBuffers; ++i) {
		if (m_fallbackMappedPtr[i] && m_fallbackFenceValues[i] <= completed && m_fallbackBufferSize[i] >= neededSize) {
			return i;
		}
	}

	// find empty slot
	for (int i = 0; i < kFallbackBuffers; ++i) {
		if (!m_fallbackMappedPtr[i]) {
			D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(neededSize);
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&rd,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_fallbackUploadBuffers[i])));
			D3D12_RANGE r = { 0,0 };
			ThrowIfFailed(m_fallbackUploadBuffers[i]->Map(0, &r, reinterpret_cast<void**>(&m_fallbackMappedPtr[i])));
			m_fallbackBufferSize[i] = neededSize;
			m_fallbackFenceValues[i] = 0;
			return i;
		}
	}
	// no slot available
	return -1;
}

// CPU -> Upload Buffer 복사
void MeshBuffer::StageBuffers(ID3D12Device* device, ID3D12Fence* graphicsFence, const MeshData& data)
{
	m_vertexCount = UINT(data.vertices.size());
	m_indexCount = UINT(data.indices.size());
	const UINT64 vbSize = m_vertexCount * UINT(sizeof(Vertex));
	const UINT64 ibSize = m_indexCount * UINT(sizeof(uint32_t));

	UINT64 uploadOffset = UINT64_MAX;
	uint8_t* uploadPtr = nullptr;
	UINT64 totalSize = AlignUp64(vbSize, 256) + AlignUp64(ibSize, 256);

	if (g_uploadRing)
	{
		uploadPtr = g_uploadRing->Allocate(totalSize, uploadOffset);
	}

	if (uploadPtr && uploadOffset != UINT64_MAX)
	{
		memcpy(uploadPtr, data.vertices.data(), vbSize);
		UINT64 idxOffsetWithin = AlignUp64(vbSize, 256);
		memcpy(uploadPtr + idxOffsetWithin, data.indices.data(), ibSize);

		m_vertexUploadOffset = uploadOffset;
		m_indexUploadOffset = uploadOffset + idxOffsetWithin;
		m_activeFallbackIndex = -1;
	}
	else
	{
		int slot = FindOrCreateFallbackSlot(device, graphicsFence, totalSize);
		if (slot >= 0) {
			uint8_t* dst = m_fallbackMappedPtr[slot];
			memcpy(dst, &data.vertices, vbSize);
			memcpy(dst + AlignUp64(vbSize, 256), &data.indices, ibSize);
			m_vertexUploadOffset = 0; m_indexUploadOffset = AlignUp64(vbSize, 256);
			m_activeFallbackIndex = slot;
		}
		else
		{
			g_uploadRing->ReclaimCompleted(graphicsFence->GetCompletedValue());
		}
	}
}

void MeshBuffer::CommitBuffers(ID3D12GraphicsCommandList* cmdList, const MeshData& data)
{
#if PIX_DEBUGMODE
	PIXBeginEvent(cmdList, PIX_COLOR(0, 255, 0), L"MeshBuffer:CommitBuffers");
#endif

	m_vertexCount = UINT(data.vertices.size());
	m_indexCount = UINT(data.indices.size());

	// Vertex
	if (m_vertexCount > 0)
	{
		const UINT vbSize = m_vertexCount * UINT(sizeof(Vertex));
		auto before = m_justInitailized ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), before, D3D12_RESOURCE_STATE_COPY_DEST));

		if (m_activeFallbackIndex >= 0)
		{
			cmdList->CopyBufferRegion(m_vertexBuffer.Get(), 0, m_fallbackUploadBuffers[m_activeFallbackIndex].Get(), m_vertexUploadOffset, vbSize);
		}
		else
		{
			cmdList->CopyBufferRegion(m_vertexBuffer.Get(), 0, g_uploadRing->GetResource(), m_vertexUploadOffset, vbSize);
		}

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	}

	// Index
	if (m_indexCount > 0)
	{
		const UINT ibSize = m_indexCount * UINT(sizeof(uint32_t));
		auto before = m_justInitailized ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_INDEX_BUFFER;
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), before, D3D12_RESOURCE_STATE_COPY_DEST));

		if (m_activeFallbackIndex >= 0)
		{
			cmdList->CopyBufferRegion(m_vertexBuffer.Get(), 0, m_fallbackUploadBuffers[m_activeFallbackIndex].Get(), m_indexUploadOffset, ibSize);
		}
		else
		{
			cmdList->CopyBufferRegion(m_indexBuffer.Get(), 0, g_uploadRing->GetResource(), m_indexUploadOffset, ibSize);
		}

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
	}

#if PIX_DEBUGMODE
	PIXEndEvent(cmdList);
#endif
}

void MeshBuffer::ResizeIfNeededAndCommit(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& data)
{
	UINT vc = UINT(data.vertices.size());
	UINT ic = UINT(data.indices.size());

	const UINT vbSize = vc * UINT(sizeof(Vertex));
	const UINT ibSize = ic * UINT(sizeof(uint32_t));

	auto needResize = [&](ComPtr<ID3D12Resource> const& r, UINT need) {
		return !r || r->GetDesc().Width < need;
		};

	if (needResize(m_vertexBuffer, vbSize) || needResize(m_indexBuffer, ibSize))
	{
		Initialize(device, std::max<UINT>(vbSize, 1), std::max<UINT>(ibSize, 1));
		m_reservedVertexCount = vc;
		m_reservedIndexCount = ic;
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

void MeshBuffer::CommitBuffers(ID3D12GraphicsCommandList* cmdList, const MeshData& data, ID3D12Resource* uploadResource, UINT64 uploadOffset)
{
	const UINT vertexBufferSize = UINT(data.vertices.size() * sizeof(Vertex));
	const UINT indexBufferSize = UINT(data.indices.size() * sizeof(uint32_t));

	// aligned offsets inside the upload block
	const UINT64 vbOffset = uploadOffset;
	const UINT64 ibOffset = uploadOffset + ((vertexBufferSize + 255) & ~255);

	// Vertex: transition -> copy -> transition
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));

	cmdList->CopyBufferRegion(m_vertexBuffer.Get(), 0, uploadResource, vbOffset, vertexBufferSize);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Index: similar
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_indexBuffer.Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));

	cmdList->CopyBufferRegion(m_indexBuffer.Get(), 0, uploadResource, ibOffset, indexBufferSize);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	// update stored counts
	m_vertexCount = UINT(data.vertices.size());
	m_indexCount = UINT(data.indices.size());
}

void MeshBuffer::Initialize(ID3D12Device* device, UINT vertexBufferSize, UINT indexBufferSize)
{
	// Push_back Old Buffer to DeletionSink
	if (m_deletionSink)
	{
		if (m_vertexBuffer) m_deletionSink->push_back(std::move(m_vertexBuffer));
		if (m_indexBuffer) m_deletionSink->push_back(std::move(m_indexBuffer));
	}

	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

	// Create Heap
	// Buffer는 읽기 전용으로 생성, 쓰기 전용의 버퍼로부터 복사하여 사용한다.
	// 읽기 전용 버퍼와 쓰기 전용 버퍼를 분리하면 다음의 효과를 얻을 수 있음.
	// 1. CPU->GPU 업로드 시 Upload Heap을 사용하면 Map과 memcpy 호출로 데이터를 쓸 수 있음.
	// 2. GPU 렌더링 시 GPU는 Default Heap을 가장 빨리 읽음.
	// 3. CPU가 데이터를 쓰는 동안 GPU가 사용 중인 리소스에 접근 대기하는 대기 시간(파이프라인 동기화 시간)을 없앨 수 있음.
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

void Mesh::StageBuffers(ID3D12Device* device, ID3D12Fence* graphicsFence)
{
	m_buffer.StageBuffers(device, graphicsFence, m_data);
	BuildTriBounds();
}

void Mesh::CommitBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	m_buffer.ResizeIfNeededAndCommit(device, cmdList, m_data);
}

void Mesh::UpdateData(ID3D12Device* device, ID3D12Fence* graphicsFence, const MeshData& meshData)
{
	m_data = meshData;
	StageBuffers(device, graphicsFence);
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
