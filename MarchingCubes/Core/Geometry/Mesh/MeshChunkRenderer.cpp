#include "pch.h"
#include "MeshChunkRenderer.h"
#include "Core/Rendering/UploadContext.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>

MeshChunkRenderer::MeshChunkRenderer(ID3D12Device* device) :
	m_mappedObjectCB(nullptr)
{
	m_worldMat = XMMatrixIdentity();
	XMMATRIX worldMatTrans = XMMatrixTranspose(m_worldMat);
	XMStoreFloat4x4(&m_objectCBData.worldMatrix, worldMatTrans);
	XMMATRIX worldMatTransInv = XMMatrixInverse(nullptr, worldMatTrans);
	XMStoreFloat4x4(&m_objectCBData.worldInvMatrix, worldMatTransInv);
	CreateObjectConstantsBuffer(device);
}

void MeshChunkRenderer::ApplyUpdates(ID3D12Device* device, const std::vector<ChunkUpdate>& ups)
{
	auto buildTriBounds = [](const GeometryData& meshdata, BoundingBox& OutBounds){
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
				m_chunks.erase(u.key);
				auto itDraw = m_chunkDrawables.find(u.key);
				if (itDraw != m_chunkDrawables.end())
				{
					m_pendingDeletes.push_back(std::move(itDraw->second));
					m_chunkDrawables.erase(itDraw);
				}
			}
		}
		else
		{
			if (iter != m_chunks.end())
			{
				ChunkSlot& chunkSlot = iter->second;
				chunkSlot.bNeedsUpload = true;
				chunkSlot.meshData = u.md;
				buildTriBounds(chunkSlot.meshData, chunkSlot.triBound);

				if (m_chunkDrawables.find(u.key) == m_chunkDrawables.end())
				{
					m_chunkDrawables.emplace(u.key, std::make_unique<ChunkDrawable>(this, u.key));
				}
			}
			else
			{
				ChunkSlot chunkSlot;
				chunkSlot.bNeedsUpload = true;
				chunkSlot.meshData = u.md;
				buildTriBounds(chunkSlot.meshData, chunkSlot.triBound);
				//chunkSlot.meshBuffer.CreateBuffers(device, u.md);
				m_chunks.insert_or_assign(u.key, chunkSlot);
				m_chunkDrawables.emplace(u.key, std::make_unique<ChunkDrawable>(this, u.key));
			}
		}
	}
}

void MeshChunkRenderer::Clear()
{
	m_chunks.clear();
	m_pendingDeletes.reserve(m_chunkDrawables.size());
	for (auto& [key, chunk] : m_chunkDrawables)
	{
		m_pendingDeletes.emplace_back(std::move(chunk));
	}
	m_chunkDrawables.clear();
}

std::vector<IDrawable*> MeshChunkRenderer::GetChunkDrawables()
{
	std::vector<IDrawable*> out;
	out.reserve(m_chunkDrawables.size());
	for (auto& [k, uptr] : m_chunkDrawables)
	{
		// check active slot
		auto it = m_chunks.find(k);
		if (it == m_chunks.end()) continue;

		out.push_back(uptr.get());
	}
	return out;
}

std::vector<BoundingBox> MeshChunkRenderer::GetBoundingBox() const
{
	std::vector<BoundingBox> out;
	out.reserve(m_chunks.size());

	for (const auto& [_, slot] : m_chunks)
	{
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

std::vector<const GeometryData*> MeshChunkRenderer::GetCPUData() const
{
	std::vector<const GeometryData*> out;
	out.reserve(m_chunks.size());
	for (const auto& [_, slot] : m_chunks)
	{
		if (slot.meshData.indices.empty()) continue;

		out.push_back(&slot.meshData);
	}

	return out;
}

void MeshChunkRenderer::CreateObjectConstantsBuffer(ID3D12Device* device)
{
	static const UINT objectConstantBufferSize = AlignUp(sizeof(ObjectConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(objectConstantBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_objectCB.ReleaseAndGetAddressOf())
	));
	NAME_D3D12_OBJECT_ALIAS(m_objectCB, L"MeshChunkRenderer");

	// Map & CreateUploadBuffer Constant Buffer
	ThrowIfFailed(m_objectCB->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedObjectCB)));
	memcpy(m_mappedObjectCB, &m_objectCBData, sizeof(m_objectCBData));
}

DrawBindingInfo MeshChunkRenderer::ChunkDrawable::GetDrawBinding() const
{
	DrawBindingInfo info{};
	auto it = m_parent->m_chunks.find(m_key);
	if (it == m_parent->m_chunks.end()) return info;

	const ChunkSlot& slot = it->second;
	if (slot.meshData.indices.empty()) return info;

	info.vbv = slot.meshBuffer.GetVBV();
	info.ibv = slot.meshBuffer.GetIBV();
	info.topology = slot.meshBuffer.GetTopology();
	info.indexCount = static_cast<UINT>(slot.meshData.indices.size());
	info.objectCBGpuVA = m_parent->m_objectCB ? m_parent->m_objectCB->GetGPUVirtualAddress() : 0ull;
	info.material = m_parent->m_material.get();
	return info;
}

ObjectConstants MeshChunkRenderer::ChunkDrawable::GetObjectConstants() const
{
	return m_parent->m_objectCBData;
}
