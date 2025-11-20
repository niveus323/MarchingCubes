#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <unordered_map>
#include <DirectXCollision.h>
#include <format>

class UploadContext;

class MeshChunkRenderer final
{
public:
	explicit MeshChunkRenderer();
	explicit MeshChunkRenderer(const ObjectConstants& cb);
	void ApplyUpdates(ID3D12Device* device, const std::vector<ChunkUpdate>& ups);
	void Clear();

	// Constants
	void SetMaterial(uint32_t index) { m_objectCBData.materialIndex = index; }
	inline XMMATRIX GetWorldMatrix() const { return m_worldMat; }
	inline XMMATRIX GetWorldInvMatrix() const { return DirectX::XMMatrixInverse(nullptr, m_worldMat); }

	std::vector<IDrawable*> GetChunkDrawables();
	std::vector<BoundingBox> GetBoundingBox() const;
	std::vector<const GeometryData*> GetCPUData() const;
	std::vector<std::unique_ptr<IDrawable>> GetPendingDeletes() { return std::move(m_pendingDeletes); }

	void SetDebugName(const std::string& debugName) 
	{ 
		m_debugName = debugName; 
		for (auto& [k, chunk] : m_chunkDrawables)
		{
			if (chunk) chunk->SetDebugName(m_debugName);
		}
	}

private:
	// inner class
	struct ChunkDrawable : public IDrawable
	{
		ChunkDrawable(MeshChunkRenderer* parent, const ChunkKey& key) : m_parent(parent), m_key(key) 
		{
			SetDebugName(m_parent->m_debugName);
		}

		DrawBindingInfo GetDrawBinding() const override;
		ObjectConstants GetObjectConstants() const override;

		const GeometryData* GetCPUData() const override
		{
			auto it = m_parent->m_chunks.find(m_key);
			if (it == m_parent->m_chunks.end()) return nullptr;
			return &it->second.meshData;
		}

		void SetCPUData(const GeometryData& data) override
		{
			auto it = m_parent->m_chunks.find(m_key);
			if (it != m_parent->m_chunks.end()) it->second.meshData = data;
		}

		void SetCPUData(GeometryData&& data) override
		{
			auto it = m_parent->m_chunks.find(m_key);
			if (it != m_parent->m_chunks.end()) it->second.meshData = std::move(data);
		}

		GeometryBuffer* GetGPUBuffer() override
		{
			auto it = m_parent->m_chunks.find(m_key);
			if (it == m_parent->m_chunks.end()) return nullptr;
			return &it->second.meshBuffer;
		}

		void SetUploadPending(bool pending) override
		{
			auto it = m_parent->m_chunks.find(m_key);
			if (it != m_parent->m_chunks.end()) it->second.bNeedsUpload = pending;
		}

		bool IsUploadPending() const override
		{
			auto it = m_parent->m_chunks.find(m_key);
			return (it != m_parent->m_chunks.end()) ? it->second.bNeedsUpload : false;
		}

		const char* GetDebugName() const override
		{
			return m_cachedDebugName.c_str();
		}

		void SetDebugName(const std::string& name) override
		{
			m_cachedDebugName = std::format("{}({},{},{})", name, m_key.x, m_key.y, m_key.z);
		}

		MeshChunkRenderer* m_parent;
		ChunkKey m_key;
		mutable std::string m_cachedDebugName;
	};

	struct ChunkSlot
	{
		GeometryData meshData;
		GeometryBuffer meshBuffer;
		DirectX::BoundingBox triBound;
		bool bNeedsUpload = false;
	};

	// Mesh
	std::unordered_map<ChunkKey, ChunkSlot, ChunkKeyHash> m_chunks;
	std::unordered_map<ChunkKey, std::unique_ptr<ChunkDrawable>, ChunkKeyHash> m_chunkDrawables;
	std::vector<std::unique_ptr<IDrawable>> m_pendingDeletes;
	
	// Object Constants Buffer
	DirectX::XMMATRIX m_worldMat;
	ObjectConstants m_objectCBData{}; // GPU Object Constants
	UINT8* m_mappedObjectCB = nullptr;

	// Debug
	std::string m_debugName = "MeshChunk";
};

