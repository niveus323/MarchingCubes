#pragma once
#include "CommonMemory.h"

struct RetiredBlock
{
	uint64_t offset = 0;
	uint64_t size = 0;
	uint64_t fence = 0;

	RetiredBlock(uint64_t off, uint64_t sz, uint64_t f) :offset(off), size(sz), fence(f) {}
	RetiredBlock(BufferBlock b, uint64_t f) : offset(b.offset), size(b.size), fence(f) {}
};

class GPUBufferPool
{
public:
	GPUBufferPool(ID3D12Device* device, uint64_t totalSize, const wchar_t* debugName = L"GPUBufferPool");
	~GPUBufferPool();
	bool SubAlloc(ID3D12Device* device, uint64_t bytes, uint64_t align, BufferHandle& out, const char* owner = nullptr);
	void FreeLater(const BufferHandle& r, uint64_t fence); // retire만 표시
	void Reclaim(uint64_t completedFence); // retireFence ≤ completedFence 회수

	ID3D12Resource* GetResource() const { return m_buffer.Get(); }
	uint64_t GetCapacity() const { return m_capacity; }
	std::vector<BufferBlock> GetFreeBlocks() const { return m_free; }
	std::vector<BufferBlock> GetAllocatedBlocks() const { return m_allocated; }
private:
	void MergeFree();

private:
	ComPtr<ID3D12Resource> m_buffer;
	uint64_t m_capacity = 0;
	std::vector<BufferBlock> m_free;
	std::vector<RetiredBlock> m_retired;
	std::vector<BufferBlock> m_allocated;
};

