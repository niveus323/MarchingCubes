#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

class UploadContext;

class SDFVolume3D
{
public:
	SDFVolume3D(ID3D12Device* device, UploadContext* uploadContext);

	void uploadFromGRD(ID3D12GraphicsCommandList* cmd, const SdfField<float>* grid);

	ID3D12Resource* density() const { return m_density3D.Get(); }

private:
	void EnsureDensityTex(const uint32_t dimX, const uint32_t dimY, const uint32_t dimZ);
private:
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_density3D;
	UploadContext* m_uploadContext;

};

