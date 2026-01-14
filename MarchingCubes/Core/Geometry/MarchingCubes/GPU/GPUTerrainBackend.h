#pragma once
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <memory>
#include <array>

class DescriptorAllocator;
class UploadContext;
class SDFVolume3D;
class GPUBrushCS;
class GPUMarchingCubesCS;

struct OutVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT4 tangent;
	int idA; // 엣지 A 인덱스 번호
	int idB; // 엣지 B 인덱스 번호
};

struct OutTriangle
{
	OutVertex C;
	OutVertex B;
	OutVertex A;
	uint32_t chunkIdx;
};

class GPUTerrainBackend : public ITerrainBackend
{
public:
	explicit GPUTerrainBackend(ID3D12Device* device, const GridDesc& gridDesc, DescriptorAllocator* descriptorAllocator);
	~GPUTerrainBackend();

	// ITerrainBackend을(를) 통해 상속됨
	virtual void setGridDesc(const GridDesc& desc) override;
	virtual void setFieldPtr(std::shared_ptr<SdfField> grid) override;
	virtual void RequestBrush(const BrushRequest& r) override;
	virtual void RequestRemesh(const std::set<ChunkKey>& chunkSet) override;
	virtual bool tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates) override;
	virtual bool HasRequests() const override { return m_fieldDirty || !m_brushQueue.empty() || !m_pendingRemeshChunks.empty(); }
	
	void ExecuteCompute(uint32_t frameIndex, UploadContext* uploadContext, DescriptorAllocator* descriptorAllocator);

private:
	void ensureTriangleBuffer();
	void computeNumChunks();
	void ensureRBSlot(uint32_t slot);
	void resetRBSlot(uint32_t slot);
	void prepareComputeEncoding();
	void finishComputeEncoding();
	void encodeFieldUpload(UploadContext* context);
	void encodeBrushPass(uint32_t frameIndex, UploadContext* uploadContext, const BrushRequest& req, DirectX::XMUINT3& regionMin, DirectX::XMUINT3& regionMax);
	void encodeRemeshPass(uint32_t frameIndex, UploadContext* uploadContext, const std::set<ChunkKey>& chunkSet, const DirectX::XMUINT3& regionMin, const DirectX::XMUINT3& regionMax);

	static XMUINT3 computeBrushCenter(const DirectX::XMFLOAT3& hitpos, const DirectX::XMFLOAT3& gridorigin, const float cellsize);
	static void computeBrushRegionCells(const GridDesc& grid, const DirectX::XMUINT3& brushCenter, const float brushRadius, DirectX::XMUINT3& outRegionMin, DirectX::XMUINT3& outRegionMax);
	static void computeChunkAlignedRegion(const XMUINT3& resolution, const XMUINT3& brushRegionMin, const XMUINT3& brushRegionMax, XMUINT3& outRegionMin, XMUINT3& outRegionMax);

private:
	static constexpr uint32_t m_ring = 3;
	static constexpr size_t kRBFrameCount = 2; // N 프레임 [0] Dispatch -> N+1 프레임에서 [0] Fetch, [1] Dispatch
	static constexpr int s_chunkcubes = 16; // threadgroup 8x8x8 정렬

	ID3D12Device* m_device = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;

	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator[kRBFrameCount];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent;
	uint64_t m_fenceValues;
	uint64_t m_lastSubmitFenceValues;

	std::unique_ptr<SDFVolume3D>        m_vol;
	std::unique_ptr<GPUBrushCS>         m_brush;
	std::unique_ptr<GPUMarchingCubesCS> m_mc;

	struct RBRound {
		ComPtr<ID3D12Resource> rbTriangles;
		ComPtr<ID3D12Resource> rbCount;
		uint32_t count = 0;
	};
	std::array<RBRound, kRBFrameCount> m_rb{};
	uint32_t m_rbCursor = 0;

	ComPtr<ID3D12Resource> m_outBuffer;
	ComPtr<ID3D12Resource> m_outCounter;
	static const uint32_t m_triCapPerChunk = 5*16*16*16; // 청크당 삼각형

	GridDesc m_grid{};
	DirectX::XMUINT3 m_numChunkAxis{};
	uint32_t m_numChunks = 0;

	std::shared_ptr<SdfField>     m_gridData;

	std::vector<BrushRequest> m_brushQueue;
	std::set<ChunkKey> m_pendingRemeshChunks;

	bool m_fieldDirty = false;
	bool m_needsFetch = false;

	uint32_t m_triTableSlot = UINT32_MAX;
};

