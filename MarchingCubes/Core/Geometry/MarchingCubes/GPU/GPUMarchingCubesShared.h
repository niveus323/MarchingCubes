#pragma once
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Utils/DXHelper.h"
#include "Core/Rendering/PSO/DescriptorRing.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

struct SDFVolumeView
{
	ID3D12Resource* tex = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE srv{}; // Density3D SRV(t1, u1)
	D3D12_GPU_DESCRIPTOR_HANDLE uav{}; // Density3D UAV(u1)
	GridDesc grid{};
	uint32_t chunkCubes;
	XMUINT3 numChunkAxis;
};