#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

struct alignas(16) GridCBData {
	XMUINT3 resolution;
	float scale;
	XMFLOAT3 chunkOrigin;
	float isoValue;
	XMUINT3 numChunkAxis;
	UINT chunkCubes;
	XMUINT3 groupsPerChunk;
	UINT chunkCapacity;
};

struct GPUMCEncodingContext
{
	ID3D12Device* device = nullptr;
	ID3D12GraphicsCommandList* cmd = nullptr;
	const SDFVolumeView& vol;
	const FrameAlloc& fa;
	const RemeshRequest& req;
	XMUINT3 numChunkAxis;
	uint32_t chunkCubes = 16;
	uint32_t chunkCapacity = 4096;

	GPUMCEncodingContext(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const SDFVolumeView& vol, const FrameAlloc& fa, const RemeshRequest& req, const DirectX::XMUINT3& numChunkAxis, UINT chunkCubes, UINT chunkCapacity): device(device), cmd(cmd), vol(vol), fa(fa), req(req), numChunkAxis(numChunkAxis), chunkCubes(chunkCubes), chunkCapacity(chunkCapacity){}
	GPUMCEncodingContext(const GPUMCEncodingContext&) = delete;
	GPUMCEncodingContext& operator=(const GPUMCEncodingContext&) = delete;
};

class GPUMarchingCubesCS
{
public:
	GPUMarchingCubesCS(ID3D12Device* device);
	
	void encode(const GPUMCEncodingContext& context);

	ID3D12RootSignature* mcRootSig() const { return m_mcRootSig.Get(); }
	ID3D12PipelineState* mcPso() const { return m_mcPso.Get(); }
	ID3D12Resource* triTable() const { return m_triDefault.Get(); }

private:
	void ensurePipeline(ID3D12Device* device);
	void ensureTable(ID3D12Device* device);

private:
	ComPtr<ID3D12RootSignature>    m_mcRootSig;
	ComPtr<ID3D12PipelineState>    m_mcPso;

	ComPtr<ID3D12CommandSignature> m_mcCmdSig;

	// Resources
	ComPtr<ID3D12Resource> m_triDefault;
	ComPtr<ID3D12Resource> m_triUpload;

	bool m_tableUploaded = false;
};

