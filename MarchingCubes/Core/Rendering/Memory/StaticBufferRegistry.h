#pragma once
#include "CommonMemory.h"
#include <map>

struct StaticRegistryInitInfo
{
	uint64_t vbSize = 32ull << 20;
	uint64_t ibSize = 16ull << 20;
};

class StaticBufferRegistry
{
public:
	explicit StaticBufferRegistry(ID3D12Device* device, StaticRegistryInitInfo info);
	StaticBufferRegistry(ID3D12Device* device, uint64_t vbSize = 32ull << 20, uint64_t ibSize = 16ull << 20);
	uint32_t CreateStatic(ID3D12Device* device, uint32_t vbBytes, uint32_t ibBytes, uint32_t vertexStride = 0, DXGI_FORMAT ibFormat = DXGI_FORMAT_R32_UINT, BufferHandle* outVB = nullptr, BufferHandle* outIB = nullptr, std::string_view debugName = "");
	void Release(uint32_t handle);

	uint64_t GetVBCapacity() const { return m_vbCapacity; }
	std::vector<BufferBlock> GetVBAllocated() const { return m_vbAllocatedList; }
	std::vector<BufferBlock> GetVBFree() const { return m_vbFreeList; }
	uint64_t GetIBCapacity() const { return m_ibCapacity; }
	std::vector<BufferBlock> GetIBAllocated() const { return m_ibAllocatedList; }
	std::vector<BufferBlock> GetIBFree() const { return m_ibFreeList; }

private:
	uint32_t CreateStaticVB(ID3D12Device* device, uint32_t vbBytes, uint32_t vertexStride = 0, std::string_view debugName = "");
	uint32_t CreateStaticIB(ID3D12Device* device, uint32_t ibBytes, DXGI_FORMAT ibFormat = DXGI_FORMAT_R32_UINT, std::string_view debugName = "");
	uint64_t AllocFromHeap(std::vector<BufferBlock>& freeList, uint32_t bytes);
	void ReleaseVB(uint32_t vbHandle);
	void ReleaseIB(uint32_t ibHandle);
	void Free(std::vector<BufferBlock>& freeList, uint64_t heapSize, uint64_t offset, uint64_t size);
	void MergeFree(std::vector<BufferBlock>& freeList);

private:
	ComPtr<ID3D12Heap> m_VBHeap;
	ComPtr<ID3D12Heap> m_IBHeap;

	struct VBEntry
	{
		ComPtr<ID3D12Resource> res; // PlacedResource
		uint32_t stride = 0;
		uint32_t refCount = 0;
		uint64_t heapOffset = UINT64_MAX;
		std::string debugName = "VBEntry";
		bool bAlive = false;
	};
	struct IBEntry
	{
		ComPtr<ID3D12Resource> res; // PlacedResource
		DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
		uint32_t refCount = 0;
		uint64_t heapOffset = UINT64_MAX;
		std::string debugName = "IBEntry";
		bool bAlive = false;
	};
	struct ObjectEntry
	{
		uint32_t vbID = 0;
		uint32_t ibID = 0;
		uint32_t refCount = 0;
		std::string debugName = "ObjectEntry";
		bool bAlive = false;
	};

	std::vector<VBEntry> m_vbEntries;
	std::vector<IBEntry> m_ibEntries;
	std::vector<ObjectEntry> m_objectEntries;

	uint64_t m_vbCapacity = 0;
	uint64_t m_ibCapacity = 0;

	std::vector<BufferBlock> m_vbFreeList;
	std::vector<BufferBlock> m_vbAllocatedList;
	std::vector<BufferBlock> m_ibFreeList;
	std::vector<BufferBlock> m_ibAllocatedList;
};

