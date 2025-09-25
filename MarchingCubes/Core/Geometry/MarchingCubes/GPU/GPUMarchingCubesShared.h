#pragma once
#include "Core/Geometry/Mesh.h"
#include "Core/Utils/DXHelper.h"
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
	DescriptorHelper::DescriptorRing* descRing = nullptr;
	ConstantBufferHelper::CBRing* cbRing = nullptr;
	UINT ringCursor;
};

// kSlot_t1Àº static
enum EMCSlots : UINT {
	kSlot_t1 = 0, // density SRV
	kSlot_u0, // OutTriangle UAV
	kSlot_u1, // RW density3D UAV
	kSlot_CountPerFrame
};