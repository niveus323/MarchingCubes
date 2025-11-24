#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

struct alignas(16) BrushCBData {
	float brushRadius;
	float brushWeight; // Add, Sub에 맞춰 +-값 app에서 넣어준다.
	float deltaTime;
	int _padding0;
	
	XMUINT3 gridCells;
	int _padding1;

	XMUINT3 brushCenter;
	int _padding2;
	
	XMUINT3 regionCellMin;
	int _padding3;
	XMUINT3 regionCellMax;
	int _padding4;
};

struct GPUBrushEncodingContext
{
	ID3D12GraphicsCommandList* cmd = nullptr;
	const SDFVolumeView& vol;
	XMUINT3 regionMin;
	XMUINT3 regionMax;
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = 0;
};

class GPUBrushCS
{
public:
	GPUBrushCS(ID3D12Device* device);

	void encode(const GPUBrushEncodingContext& context);

	ID3D12RootSignature* brushRootSig() const { return m_brushRootSignature.Get(); }
	ID3D12PipelineState* brushPso() const { return m_brushPso.Get(); }

private:
	void ensureRootSignature(ID3D12Device* device);
	void ensurePipeline(ID3D12Device* device);
	
	static DirectX::XMUINT3 computeBrushDispatchGroups(const DirectX::XMUINT3& regionMin, const DirectX::XMUINT3& regionMax);

private:
	// BrushCS
	ComPtr<ID3D12RootSignature> m_brushRootSignature;
	ComPtr<ID3D12PipelineState> m_brushPso;
};

