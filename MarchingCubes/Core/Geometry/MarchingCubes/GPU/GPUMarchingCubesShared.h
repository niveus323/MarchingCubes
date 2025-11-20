#pragma once
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Utils/DXHelper.h"
#include "Core/Rendering/PSO/DescriptorRing.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

struct SDFVolumeView
{
	ID3D12Resource* tex = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE srv{};
	D3D12_GPU_DESCRIPTOR_HANDLE uav{};
	GridDesc grid{};
	uint32_t chunkCubes;
	XMUINT3 numChunkAxis;
};

struct FrameAlloc {
	DescriptorRing* descRing = nullptr;
	ConstantBufferHelper::CBRing* cbRing = nullptr;
	uint32_t ringCursor;
};

// kSlot_t1Àº static
enum EMCSlots : uint32_t {
	kSlot_t1 = 0, // density SRV
	kSlot_u0, // OutTriangle UAV
	kSlot_u1, // RW density3D UAV
	kSlot_CountPerFrame
};