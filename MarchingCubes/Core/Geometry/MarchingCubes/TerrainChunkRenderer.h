#pragma once
#include "Core/Geometry/Mesh.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <unordered_map>

class TerrainChunkRenderer final : public IDrawable
{
public:
	TerrainChunkRenderer(ID3D12Device* device);
	void ApplyUpdates(ID3D12Device* device, ID3D12Fence* graphicsFence, std::vector<ComPtr<ID3D12Resource>>* sink,const std::vector<ChunkUpdate>& ups);
	void UploadData(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, std::vector<std::pair<UINT64, UINT64>>& outAllocations);
	void Clear();

	// IDrawable을(를) 통해 상속됨
	void Draw(ID3D12GraphicsCommandList* cmdList) const override;
	void SetConstantsBuffers(ID3D12GraphicsCommandList* cmdList) const override;

	// Constants
	void SetMaterial(std::shared_ptr<Material> mat) { m_material = std::move(mat); }
	inline XMMATRIX GetWorldMatrix() { return m_worldMat; }
	inline XMMATRIX GetWorldInvMatrix() { return DirectX::XMMatrixInverse(nullptr, m_worldMat); }

	std::vector<BoundingBox> GetBoundingBox() const;
	std::vector<const MeshData*> GetMeshData() const;

private:
	void CreateObjectConstantsBuffer(ID3D12Device* device);

private:
	struct ChunkKeyHash {
		size_t operator()(const ChunkKey& k) const noexcept {
			return (size_t)k.x ^ ((size_t)k.y << 21) ^ ((size_t)k.z << 42);
		}
	};

	struct ChunkSlot
	{
		MeshData meshData;
		MeshBuffer meshBuffer;
		DirectX::BoundingBox triBound;
		bool active = false;
		bool bNeedsUpload = false;
	};

	// Mesh
	std::unordered_map<ChunkKey, ChunkSlot, ChunkKeyHash> m_chunks;
	
	// Object Constants Buffer
	DirectX::XMMATRIX m_worldMat;
	ObjectConstants m_objectCBData{}; // GPU Object Constants
	ComPtr<ID3D12Resource> m_objectCB;
	UINT8* m_mappedObjectCB;

	// Material
	std::shared_ptr<Material> m_material; // Material은 App 클래스에서 공유받아 bind만 해준다.
};

