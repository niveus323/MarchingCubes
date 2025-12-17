#include "pch.h"
#include "MeshChunkRenderer.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/RenderSystem.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>

MeshChunkRenderer::MeshChunkRenderer()
{
}

void MeshChunkRenderer::ApplyUpdates(UploadContext* uploadContext, const std::vector<ChunkUpdate>& ups)
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

	if (!uploadContext) return;

	for (const auto& u : ups)
	{
		// 1. 빈 청크거나 인덱스가 없으면 삭제
		if (u.empty || u.md.indices.empty())
		{
			m_chunks.erase(u.key);
			continue;
		}

		// 2. 슬롯 확보
		ChunkSlot& slot = m_chunks[u.key];
		slot.meshData = u.md;
		slot.indexCount = static_cast<uint32_t>(u.md.indices.size());

		// 3. 바운딩 박스 계산
		if (!u.md.vertices.empty())
		{
			DirectX::BoundingBox::CreateFromPoints(slot.bounds, u.md.vertices.size(), &u.md.vertices[0].pos, sizeof(Vertex));
		}

		// 4. 버퍼 업로드 요청 (UploadContext가 중복 체크 및 재할당 담당)
		std::string debugName = std::format("Chunk_{}_{}_{}", u.key.x, u.key.y, u.key.z);
		uploadContext->UploadGeometry(&slot.buffer, u.md, debugName);
	}
}

void MeshChunkRenderer::Submit(RenderSystem* renderSystem, const DirectX::XMFLOAT4X4& worldMatrix, const MaterialInstance& material)
{
	if (!renderSystem || m_chunks.empty()) return;

	for (auto& [key, slot] : m_chunks)
	{
		if (slot.indexCount == 0) continue;

		RenderItem item;
		item.meshBuffer = &slot.buffer;
		item.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item.indexCount = slot.indexCount;
		item.indexOffset = 0;
		item.baseVertexLocation = 0;
		item.worldMatrix = worldMatrix;
		item.materialIndex = material.index;
#ifdef _DEBUG
		item.debugName = std::format("Chunk({},{},{})", key.x, key.y, key.z);
#endif

		renderSystem->SubmitRenderItem(item, material.psoName);
	}
}

void MeshChunkRenderer::Clear()
{
	m_chunks.clear();
}

std::vector<BoundingBox> MeshChunkRenderer::GetBoundingBox() const
{
	std::vector<DirectX::BoundingBox> out;
	out.reserve(m_chunks.size());
	for (const auto& [_, slot] : m_chunks)
	{
		out.push_back(slot.bounds);
	}
	return out;
}

std::vector<ChunkSlot*> MeshChunkRenderer::GetChunkSlots()
{
	std::vector<ChunkSlot*> validSlots;
	validSlots.reserve(m_chunks.size());

	for (auto& [key, slot] : m_chunks)
	{
		if (slot.indexCount > 0)
		{
			validSlots.push_back(&slot);
		}
	}
	return validSlots;
}
