#pragma once
#include <d3d12.h>
#include <cstdint>

// GPU Allocation
struct ResourceSlice
{
	ID3D12Resource* res = nullptr;
	UINT64 offset = 0;
	UINT64 size = 0;
	UINT64 retireFence = 0;
	D3D12_GPU_VIRTUAL_ADDRESS gpuVA = 0; // For CB
	uint8_t* cpuPtr = nullptr; // For CB & Staging
};

struct BufferBlock
{
	UINT64 offset = 0;
	UINT64 size = 0;
	const char* owner = "";

	BufferBlock(UINT64 off, UINT64 sz, const char* o = nullptr) : offset(off), size(sz), owner(o) {}
};

static constexpr UINT PROMOTE_VB_MIN = 256 * 1024; // VB는 256KB 이상일 경우 승격
static constexpr UINT PROMOTE_IB_MIN = 64 * 1024; // IB는 64KB 이상일 경우 승격