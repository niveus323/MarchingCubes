#pragma once
#include <d3d12.h>
#include <cstdint>

// GPU Allocation
struct BufferHandle
{
	ID3D12Resource* res = nullptr;
	uint64_t offset = 0;
	uint64_t size = 0;
	uint64_t retireFence = 0;
	D3D12_GPU_VIRTUAL_ADDRESS gpuVA = 0; // For CB
	uint8_t* cpuPtr = nullptr; // For CB & Staging
};

struct BufferBlock
{
	uint64_t offset = 0;
	uint64_t size = 0;
	std::string owner = "";

	BufferBlock(uint64_t off, uint64_t sz, std::string_view o = "") : offset(off), size(sz), owner(o) {}
};

struct BufferPoolInfo
{
	std::string name;
	uint64_t capacity = 0;
	uint64_t used = 0;
	std::vector<BufferBlock> free;
	std::vector<BufferBlock> allocated;
};

struct DedicatedBufferInfo
{
	std::string type;       // "Promoted" or "Fallback"
	std::string owner;      // AllocDesc::owner
	std::string usage;      // "VERTEX", "INDEX", "GENERIC" ...
	uint64_t size = 0;
	uint64_t fenceValue = 0;// 현재 GPU 처리 상태 확인용
	bool isLive = false;    // 현재 참조되고 있는지(RefCount > 0)
};

static constexpr uint32_t PROMOTE_VB_MIN = 256 * 1024; // VB는 256KB 이상일 경우 승격
static constexpr uint32_t PROMOTE_IB_MIN = 64 * 1024; // IB는 64KB 이상일 경우 승격