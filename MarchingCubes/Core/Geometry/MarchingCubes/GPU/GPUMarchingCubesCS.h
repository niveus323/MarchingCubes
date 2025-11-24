#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

struct alignas(16) GridCBData {
	XMUINT3  gridCells;      // = m_grid.cells
	uint32_t _padding0;

	XMFLOAT3 gridOrigin;     // = m_grid.origin
	float    isoValue;

	XMUINT3  numChunkAxis;   // = volView.numChunkAxis
	uint32_t chunkCubes;     // = volView.chunkCubes

	XMUINT3  regionCellMin;  // = 청크 정렬된 remesh 범위 (셀)
	uint32_t _padding1;
	XMUINT3  regionCellMax;
	uint32_t _padding2;
};

struct GPUMCEncodingContext
{
	ID3D12Device* device = nullptr;
	ID3D12GraphicsCommandList* cmd = nullptr;
	const SDFVolumeView& vol;
	const RemeshRequest& req;
	XMUINT3 regionCellMin;
	XMUINT3 regionCellMax;
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = 0; // b0: GridCB
	D3D12_GPU_DESCRIPTOR_HANDLE triTableSrv{}; // t0: triTable
	D3D12_GPU_DESCRIPTOR_HANDLE outBufferUav{}; // u0: OutTriangle UAV
};

class GPUMarchingCubesCS
{
public:
	GPUMarchingCubesCS(ID3D12Device* device);

	void encode(const GPUMCEncodingContext& context);

	ID3D12RootSignature* mcRootSig() const { return m_mcRootSig.Get(); }
	ID3D12PipelineState* mcPso() const { return m_mcPso.Get(); }
	ID3D12Resource* triTable() const { return m_triDefault.Get(); }
	void uploadTableIfNeeded(ID3D12GraphicsCommandList* cmd);

private:
	void ensurePipeline(ID3D12Device* device);
	void ensureTable(ID3D12Device* device);

	static DirectX::XMUINT3 computeDispatchGroupsForRegion(const uint32_t chunkcubes, const DirectX::XMUINT3& gridDim, const DirectX::XMUINT3& regionMin, const DirectX::XMUINT3& regionMax);

private:
	ComPtr<ID3D12RootSignature>    m_mcRootSig;
	ComPtr<ID3D12PipelineState>    m_mcPso;

	// Resources
	ComPtr<ID3D12Resource> m_triDefault;
	ComPtr<ID3D12Resource> m_triUpload;

	bool m_tableUploaded = false;
};

