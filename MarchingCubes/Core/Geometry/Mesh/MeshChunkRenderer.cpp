#include "pch.h"
#include "MeshChunkRenderer.h"
#include "Core/Rendering/UploadContext.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>

MeshChunkRenderer::MeshChunkRenderer()
{
	m_worldMat = XMMatrixIdentity();
	XMMATRIX worldMatTrans = XMMatrixTranspose(m_worldMat);
	DirectX::XMStoreFloat4x4(&m_objectCBData.worldMatrix, worldMatTrans);
	XMMATRIX worldMatTransInv = XMMatrixInverse(nullptr, worldMatTrans);
	DirectX::XMStoreFloat4x4(&m_objectCBData.worldInvMatrix, worldMatTransInv);
	m_objectCBData.bUseTriplanar = true;
}

MeshChunkRenderer::MeshChunkRenderer(const ObjectConstants& cb) :
	m_objectCBData(cb)
{
	m_worldMat = DirectX::XMLoadFloat4x4(&m_objectCBData.worldMatrix);
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

DrawBindingInfo MeshChunkRenderer::ChunkDrawable::GetDrawBinding() const
{
	DrawBindingInfo info{};
	auto it = m_parent->m_chunks.find(m_key);
	if (it == m_parent->m_chunks.end()) return info;

	const ChunkSlot& slot = it->second;
	if (slot.meshData.indices.empty()) return info;

	const BufferHandle& vb = slot.meshBuffer.GetCurrentVBHandle();
	info.vbv = {
		.BufferLocation = vb.res ? vb.res->GetGPUVirtualAddress() + vb.offset : 0,
		.SizeInBytes = static_cast<UINT>(vb.size),
		.StrideInBytes = static_cast<UINT>(sizeof(Vertex)),
	};

	const BufferHandle& ib = slot.meshBuffer.GetCurrentIBHandle();
	info.ibv = {
		.BufferLocation = ib.res ? (ib.res->GetGPUVirtualAddress() + ib.offset) : 0,
		.SizeInBytes = static_cast<UINT>(ib.size),
		.Format = DXGI_FORMAT_R32_UINT
	};
	info.topology = slot.meshBuffer.GetTopology();
	info.indexCount = static_cast<UINT>(slot.meshData.indices.size());
	info.objectCBGpuVA = slot.meshBuffer.GetCurrentCBHandle().gpuVA;
	return info;
}

ObjectConstants MeshChunkRenderer::ChunkDrawable::GetObjectConstants() const
{
	return m_parent->m_objectCBData;
}
