#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <unordered_map>
#include <DirectXCollision.h>
#include <ranges>

class UploadContext;
class RenderSystem;

class MeshChunkRenderer final
{
public:
	MeshChunkRenderer();
	~MeshChunkRenderer() = default;

	void ApplyUpdates(UploadContext* uploadContext, const std::vector<ChunkUpdate>& ups);
	void Submit(RenderSystem* renderSystem, const DirectX::XMFLOAT4X4& worldMatrix, const MaterialInstance& material);
	void Clear();

	std::vector<BoundingBox> GetBoundingBox() const;	
	auto GetChunkSlots() { return m_chunks | std::views::values | std::ranges::views::filter([](ChunkSlot slot) { return slot.indexCount > 0; }); }

private:
	struct ChunkSlot
	{
		GeometryBuffer buffer;
		GeometryData meshData;
		uint32_t indexCount = 0;
		DirectX::BoundingBox bounds;
	};
	// Mesh
	std::unordered_map<ChunkKey, ChunkSlot, ChunkKeyHash> m_chunks;
};

