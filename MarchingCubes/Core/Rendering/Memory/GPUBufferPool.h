#pragma once
#include "CommonMemory.h"

struct RetiredBlock
{
	UINT64 offset = 0;
	UINT64 size = 0;
	UINT64 fence = 0;

	RetiredBlock(UINT64 off, UINT64 sz, UINT64 f) :offset(off), size(sz), fence(f) {}
	RetiredBlock(BufferBlock b, UINT64 f) : offset(b.offset), size(b.size), fence(f) {}
};

class GPUBufferPool
{
public:
	GPUBufferPool(ID3D12Device* device, UINT64 totalSize, const wchar_t* debugName = L"GPUBufferPool");
	~GPUBufferPool();
	bool SubAlloc(ID3D12Device* device, UINT64 bytes, UINT64 align, ResourceSlice& out, const char* owner = nullptr);
	void FreeLater(const ResourceSlice& r, UINT64 fence);         // retire만 표시
	void Reclaim(UINT64 completedFence);                  // retireFence ≤ completedFence 회수

	ID3D12Resource* GetResource() const { return m_buffer.Get(); }                  // 큰 Default buffer
	UINT64 GetCapacity() const { return m_capacity; }
	std::vector<BufferBlock> GetFreeBlocks() const { return m_free; }
	std::vector<BufferBlock> GetAllocatedBlocks() const { return m_allocated; }
private:
	void MergeFree();

private:
	ComPtr<ID3D12Resource> m_buffer;
	UINT64 m_capacity = 0;
	std::vector<BufferBlock> m_free;
	std::vector<RetiredBlock> m_retired;
	std::vector<BufferBlock> m_allocated;
};

