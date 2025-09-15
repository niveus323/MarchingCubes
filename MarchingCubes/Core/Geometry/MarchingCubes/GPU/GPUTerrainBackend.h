#pragma once
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <memory>
#include <array>

class SDFVolume3D;
class GPUBrushCS;
class GPUMarchingCubesCS;

struct OutVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	int idA; // 엣지 A 인덱스 번호
	int idB; // 엣지 B 인덱스 번호
};

struct OutTriangle
{
	OutVertex C;
	OutVertex B;
	OutVertex A;
	UINT chunkIdx;
};

class GPUTerrainBackend : public ITerrainBackend
{
public:
	explicit GPUTerrainBackend(ID3D12Device* device, const GridDesc& gridDesc);

	// ITerrainBackend을(를) 통해 상속됨
	void setGridDesc(const GridDesc& desc) override;
	void setFieldPtr(std::shared_ptr<_GRD> grid) override;
	void requestBrush(const BrushRequest& r) override;
	void requestRemesh(const RemeshRequest& r) override;
	void encode(ID3D12GraphicsCommandList* cmd) override;
	bool tryFetch(std::vector<ChunkUpdate>&) override;
	void drainKeepAlive(std::vector<ComPtr<ID3D12Resource>>&) override;

	DescriptorHelper::DescriptorRing& descriptorRing() { return *m_descriptorRing; }
	ConstantBufferHelper::CBRing& cbRing() { return *m_cbRing; }

private:
	void ensureTriangleBuffer();
	void ensureChunkMaskBuffer();
	void ensureChunkMetaBuffer();
	void computeNumChunks();
	void ensureRBSlot(UINT slot);
	void resetRBSlot(UINT slot);
	void initChunkBuffers(ID3D12GraphicsCommandList* cmd, bool bMarkAllDitry);

private:
	static constexpr UINT m_ring = 3;
	static constexpr size_t kRBFrameCount = 2; // N 프레임 [0] Dispatch -> N+1 프레임에서 [0] Fetch, [1] Dispatch
	static constexpr int s_chunkcubes = 16; // threadgroup 8x8x8 정렬

	ID3D12Device* m_device = nullptr;

	std::unique_ptr<SDFVolume3D>        m_vol;
	std::unique_ptr<GPUBrushCS>         m_brush;
	std::unique_ptr<GPUMarchingCubesCS> m_mc;

	// Ring Buffers (DescriptorHeap, Constant)
	std::unique_ptr<DescriptorHelper::DescriptorRing> m_descriptorRing;
	std::unique_ptr<ConstantBufferHelper::CBRing> m_cbRing;
	UINT m_ringCursor = 0;

	// readback
	ComPtr<ID3D12Resource> m_chunkMaskBuffer;
	ComPtr<ID3D12Resource> m_chunkMaskUpload_Zeros; // zero-padding
	ComPtr<ID3D12Resource> m_chunkMaskUpload_Ones;

	struct ChunkMeta
	{
		UINT touched;
		UINT counter;
	};

	ComPtr<ID3D12Resource> m_chunkMetaBuffer;
	ComPtr<ID3D12Resource> m_chunkMetaUpload_Zeros; // zero-padding
	ComPtr<ID3D12Resource> m_chunkMetaUpload_Ones;
	UINT m_chunkMaskBytes = 0;
	UINT m_chunkMetaBytes = 0;

	struct RBRound {
		ComPtr<ID3D12Resource> rbMeta;
		ComPtr<ID3D12Resource> rbTriangles;
		ComPtr<ID3D12Resource> rbCount;
		bool scheduled = false;
	};
	std::array<RBRound, kRBFrameCount> m_rb{};
	UINT m_rbCursor = 0;

	ComPtr<ID3D12Resource> m_outBuffer;
	ComPtr<ID3D12Resource> m_outCounter;
	static const UINT m_triCapPerChunk = 5*16*16*16; // 청크당 삼각형

	GridDesc m_grid{};
	DirectX::XMUINT3 m_numChunkAxis{};
	UINT m_numChunks = 0;

	std::shared_ptr<_GRD>     m_gridData;
	BrushRequest m_requestedBrush{};
	RemeshRequest m_requestedRemesh{};

	bool m_fieldDirty = false;
	bool m_hasBrush = false;
	bool m_needsRemesh = false;

	std::vector<ComPtr<ID3D12Resource>> m_pendingDeletes;
};

