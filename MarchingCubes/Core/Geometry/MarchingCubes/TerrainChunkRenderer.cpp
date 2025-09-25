#include "pch.h"
#include "TerrainChunkRenderer.h"
#include <DirectXMath.h>

TerrainChunkRenderer::TerrainChunkRenderer(ID3D12Device* device) :
	m_mappedObjectCB(nullptr)
{
	m_worldMat = XMMatrixIdentity();
	XMMATRIX worldMatTrans = XMMatrixTranspose(m_worldMat);
	XMStoreFloat4x4(&m_objectCBData.worldMatrix, worldMatTrans);
	XMMATRIX worldMatTransInv = XMMatrixInverse(nullptr, worldMatTrans);
	XMStoreFloat4x4(&m_objectCBData.worldInvMatrix, worldMatTransInv);
	CreateObjectConstantsBuffer(device);
}

void TerrainChunkRenderer::ApplyUpdates(ID3D12Device* device, ID3D12Fence* graphicsFence, std::vector<ComPtr<ID3D12Resource>>* sink, const std::vector<ChunkUpdate>& ups)
{
	auto buildTriBounds = [](const MeshData& meshdata, BoundingBox& OutBounds){
		const auto& vertices = meshdata.vertices;
		if (vertices.empty())
		{
			OutBounds = {};
			return;
		}

		BoundingBox::CreateFromPoints(OutBounds, vertices.size(), &vertices[0].pos, sizeof(Vertex));
	};

	for (auto& u : ups)
	{
		auto iter = m_chunks.find(u.key);
		if (u.empty || u.md.indices.empty())
		{
			if (iter != m_chunks.end())
			{
				// 비어있도록 수정 (TODO : 일정 프레임 이상 비우면 제거하도록 수정하기)
				iter->second.active = false;
				iter->second.meshData.indices.clear();
				iter->second.meshData.vertices.clear();
				iter->second.meshBuffer.ClearCounts();
				iter->second.triBound = {};
			}
		}
		else
		{
			if (iter != m_chunks.end())
			{
				ChunkSlot& chunkSlot = iter->second;
				chunkSlot.active = true;
				chunkSlot.bNeedsUpload = true;
				chunkSlot.meshData = u.md;
				buildTriBounds(chunkSlot.meshData, chunkSlot.triBound);
				chunkSlot.meshBuffer.StageBuffers(device, graphicsFence, u.md);
			}
			else
			{
				ChunkSlot chunkSlot;
				chunkSlot.active = true;
				chunkSlot.bNeedsUpload = true;
				chunkSlot.meshBuffer.SetDeletionSink(sink);
				chunkSlot.meshData = u.md;
				buildTriBounds(chunkSlot.meshData, chunkSlot.triBound);
				chunkSlot.meshBuffer.CreateBuffers(device, u.md);
				chunkSlot.meshBuffer.StageBuffers(device, graphicsFence, u.md);
				m_chunks.insert_or_assign(u.key, chunkSlot);
			}
		}
	}
}

void TerrainChunkRenderer::UploadData(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, std::vector<std::pair<UINT64, UINT64>>& outAllocations)
{
	for (auto& [key, slot] : m_chunks)
	{
		if (slot.bNeedsUpload)
		{
			slot.meshBuffer.ResizeIfNeededAndCommit(device, cmd, slot.meshData);

			UINT64 vbOffset = slot.meshBuffer.GetVertexUploadOffset();
			UINT64 ibOffset = slot.meshBuffer.GetIndexUploadOffset();
			if (vbOffset != UINT64_MAX && ibOffset != UINT64_MAX) {
				const UINT64 vbSize = UINT64(slot.meshData.vertices.size()) * sizeof(Vertex);
				const UINT64 ibSize = UINT64(slot.meshData.indices.size()) * sizeof(uint32_t);
				const UINT64 allocSize = AlignUp64(vbSize, 256) + AlignUp64(ibSize, 256);
				outAllocations.emplace_back(vbOffset, allocSize);
			}
			slot.bNeedsUpload = false;
		}
	}
}

void TerrainChunkRenderer::Clear()
{
	m_chunks.clear();
}

void TerrainChunkRenderer::Draw(ID3D12GraphicsCommandList* cmdList) const
{
	// 전체 chunk에 대해 공통이므로 미리 Set
	SetConstantsBuffers(cmdList);
	for (auto& [key, chunk] : m_chunks)
	{
		if (!chunk.active) continue;
		chunk.meshBuffer.Draw(cmdList);
	}
}

void TerrainChunkRenderer::SetConstantsBuffers(ID3D12GraphicsCommandList* cmdList) const
{
	cmdList->SetGraphicsRootConstantBufferView(1, m_objectCB->GetGPUVirtualAddress());
	if (m_material)
	{
		m_material->BindConstant(cmdList);
	}
}

std::vector<BoundingBox> TerrainChunkRenderer::GetBoundingBox() const
{
	std::vector<BoundingBox> out;
	out.reserve(m_chunks.size());

	for (const auto& [_, slot] : m_chunks)
	{
		if (!slot.active) continue;

		const auto& verts = slot.meshData.vertices;
		if (verts.empty()) continue;

		BoundingBox bb;
		BoundingBox::CreateFromPoints(
			bb,
			verts.size(),
			&verts[0].pos,
			sizeof(Vertex)
		);
		out.push_back(bb);
	}
	return out;
}

std::vector<const MeshData*> TerrainChunkRenderer::GetMeshData() const
{
	std::vector<const MeshData*> out;
	out.reserve(m_chunks.size());
	for (const auto& [_, slot] : m_chunks)
	{
		if (!slot.active || slot.meshData.indices.empty()) continue;

		out.push_back(&slot.meshData);
	}

	return out;
}

void TerrainChunkRenderer::CreateObjectConstantsBuffer(ID3D12Device* device)
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
	memcpy(m_mappedObjectCB, &m_objectCBData, sizeof(m_objectCBData));
}