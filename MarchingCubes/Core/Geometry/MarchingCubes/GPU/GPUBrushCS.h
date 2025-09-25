#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

struct alignas(16) BrushCBData {
	int resolution;
	float radius;
	float deltaTime;
	float weight; // Add, Sub에 맞춰 +-값 app에서 넣어준다.
	XMUINT3 cells;
	int _padding0;
	XMUINT3 brushCenter;
	int _padding1;
	XMUINT3 regionMin;
	int _padding2;
	XMUINT3 regionMax;
	int _padding3;
};

struct GPUBrushEncodingContext
{
	ID3D12Device* device = nullptr;
	ID3D12GraphicsCommandList* cmd = nullptr;
	const SDFVolumeView& vol;
	const FrameAlloc& fa;
	const BrushRequest& req;
	XMUINT3 brushCenter;
	XMUINT3 regionMin;
	XMUINT3 regionMax;
};

class GPUBrushCS
{
public:
	GPUBrushCS(ID3D12Device* device);

	void encode(const GPUBrushEncodingContext& context);

	ID3D12RootSignature* brushRootSig() const { return m_brushRootSignature.Get(); }
	ID3D12PipelineState* brushPso() const { return m_brushPso.Get(); }

private:
	void ensureSignatures(ID3D12Device* device);
	void ensurePipelines(ID3D12Device* device);
	
private:
	// BrushCS
	ComPtr<ID3D12RootSignature> m_brushRootSignature;
	ComPtr<ID3D12PipelineState> m_brushPso;
};

