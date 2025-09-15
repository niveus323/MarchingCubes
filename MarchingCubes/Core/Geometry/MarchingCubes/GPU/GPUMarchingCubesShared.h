#pragma once
#include "Core/Geometry/Mesh.h"
#include "Core/Utils/DXHelper.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

#define PIX_DEBUGMODE 0

#if PIX_DEBUGMODE
#include <pix3.h>

static HMODULE sPix = PIXLoadLatestWinPixGpuCapturerLibrary();
#endif

struct SDFVolumeView
{
	ID3D12Resource* tex = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE srv{};
	D3D12_GPU_DESCRIPTOR_HANDLE uav{};
	GridDesc grid{};
};

struct FrameAlloc {
	DescriptorHelper::DescriptorRing* descRing = nullptr;
	ConstantBufferHelper::CBRing* cbRing = nullptr;
	UINT ringCursor;
};

// kSlot_t1Àº static
enum EMCSlots : UINT {
	kSlot_t1 = 0, // density SRV
	kSlot_t2, // Region SRV
	kSlot_t3, // ChunkMasks SRV
	kSlot_u0, // OutTriangle UAV
	kSlot_u1, // RW density3D UAV
	kSlot_u2, // Out Pick UAV
	kSlot_u3, // Region UAV
	kSlot_u4, // DispatchArgs UAV
	kSlot_u5, // ChunkMasks UAV
	kSlot_u6, // ChunkCounters UAV
	kSlot_CountPerFrame
};

static constexpr UINT kMetaStridebytes = 16;
static constexpr UINT kMetaOffPred64Bytes = 0;
static constexpr UINT kMetaOffCounterBytes = 8;