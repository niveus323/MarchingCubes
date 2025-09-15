#pragma once
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesShared.h"

class SDFVolume3D
{
public:
	SDFVolume3D(ID3D12Device* device);

	void uploadFromGRD(ID3D12GraphicsCommandList* cmd, const _GRD* grid, std::vector<ComPtr<ID3D12Resource>>& pendingDeletes);

	ID3D12Resource* density() const { return m_density3D.Get(); }

private:
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_density3D;
};

