#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

struct alignas(16) RegionArgsCBData {
	XMUINT3 cells;
	float cellSize;
	
	float radius;
	int halo;
	UINT chunkCubes;
	int _padding0;

	XMUINT3 brushCenter;
	int _padding1;
};

struct alignas(16) BrushCBData {
	int resolution;
	float radius;
	float deltaTime;
	float weight; // Add, Sub에 맞춰 +-값 app에서 넣어준다.
	
	XMUINT3 cells;
	int _padding0;

	XMUINT3 brushCenter;
	int _padding1;
};

struct GPUBrushEncodingContext
{
	ID3D12Device* device = nullptr;
	ID3D12GraphicsCommandList* cmd = nullptr;
	const SDFVolumeView& vol;
	const FrameAlloc& fa;
	const BrushRequest& req;
	UINT chunkCubes = 16;
};

struct GPUBrushRegionDispatchContext
{
	ID3D12Device* device = nullptr;
	ID3D12GraphicsCommandList* cmd = nullptr;
	const SDFVolumeView& vol;
	const FrameAlloc& fa;
	float radius;
	UINT chunkCubes = 16;
	DirectX::XMUINT3 brushCeneter{};
};

class GPUBrushCS
{
public:
	GPUBrushCS(ID3D12Device* device);

	void encode(const GPUBrushEncodingContext& context);
	void FillFullGridRegionAndArgs(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const GridDesc& grid, std::vector<ComPtr<ID3D12Resource>>& pendingDeletes);

	//ID3D12RootSignature* pickRootSig() const { return m_pickRootSignature.Get(); }
	//ID3D12PipelineState* pickPso() const { return m_pickPso.Get(); }

	ID3D12RootSignature* brushRootSig() const { return m_brushRootSignature.Get(); }
	ID3D12PipelineState* brushPso() const { return m_brushPso.Get(); }

	ID3D12RootSignature* brushRegionRootSig() const { return m_brushRegionRootSignature.Get(); }
	ID3D12PipelineState* brushRegionPso() const { return m_brushRegionPso.Get(); }
	ID3D12CommandSignature* brushCmdSig() const { return m_brushCmdSignature.Get(); }

	ID3D12Resource* regionBuffer() const { return m_regionBuffer.Get(); }
	ID3D12Resource* dispatchArgs() const { return m_dispatchArgsBuffer.Get(); }

private:
	void ensureSignatures(ID3D12Device* device);
	void ensurePipelines(ID3D12Device* device);
	//void ensurePickBuffer(ID3D12Device* device);
	void ensureRegionBuffer(ID3D12Device* device);
	void ensureDispatchBuffer(ID3D12Device* device);
	void ensureReadbacks(ID3D12Device* device);

	void DispatchBrushRegionCS(const GPUBrushRegionDispatchContext& context);

private:

	// BrushCS
	ComPtr<ID3D12RootSignature> m_brushRootSignature;
	ComPtr<ID3D12PipelineState> m_brushPso;

	// BrushRegionCS
	ComPtr<ID3D12RootSignature> m_brushRegionRootSignature;
	ComPtr<ID3D12PipelineState> m_brushRegionPso;
	ComPtr<ID3D12CommandSignature> m_brushCmdSignature;

	// Resources
	//ComPtr<ID3D12Resource> m_pickBuffer;
	ComPtr<ID3D12Resource> m_regionBuffer;
	ComPtr<ID3D12Resource> m_dispatchArgsBuffer;
};

