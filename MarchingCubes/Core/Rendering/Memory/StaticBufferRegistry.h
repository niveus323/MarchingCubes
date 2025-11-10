#pragma once
#include "CommonMemory.h"
#include <map>

class StaticBufferRegistry
{
public:
	StaticBufferRegistry(ID3D12Device* device, UINT64 vbBytes = 32ull << 20, UINT64 ibBytes = 16ull << 20);
	UINT CreateStatic(ID3D12Device* device, UINT64 vbBytes, UINT64 ibBytes, UINT vertexStride = 0, DXGI_FORMAT ibFormat = DXGI_FORMAT_R32_UINT, ResourceSlice* outVB = nullptr, ResourceSlice* outIB = nullptr, const char* debugName = nullptr);
	void Release(UINT handle);

	UINT64 GetVBCapacity() const { return m_vbCapacity; }
	std::vector<BufferBlock> GetVBAllocated() const { return m_vbAllocatedList; }
	std::vector<BufferBlock> GetVBFree() const { return m_vbFreeList; }
	UINT64 GetIBCapacity() const { return m_ibCapacity; }
	std::vector<BufferBlock> GetIBAllocated() const { return m_ibAllocatedList; }
	std::vector<BufferBlock> GetIBFree() const { return m_ibFreeList; }

private:
	UINT CreateStaticVB(ID3D12Device* device, UINT64 vbBytes, UINT vertexStride = 0, const char* debugName = nullptr);
	UINT CreateStaticIB(ID3D12Device* device, UINT ibBytes, DXGI_FORMAT ibFormat = DXGI_FORMAT_R32_UINT, const char* debugName = nullptr);
	UINT64 AllocFromHeap(std::vector<BufferBlock>& freeList, UINT64 bytes);
	void ReleaseVB(UINT vbHandle);
	void ReleaseIB(UINT ibHandle);
	void Free(std::vector<BufferBlock>& freeList, UINT64 heapSize, UINT64 offset, UINT64 size);
	void MergeFree(std::vector<BufferBlock>& freeList);

private:
	ComPtr<ID3D12Heap> m_VBHeap;
	ComPtr<ID3D12Heap> m_IBHeap;

	struct VBEntry
	{
		ComPtr<ID3D12Resource> res; // PlacedResource
		UINT stride = 0;
		UINT refCount = 0;
		UINT64 heapOffset = UINT64_MAX;
		std::string debugName = "VBEntry";
		bool bAlive = false;
	};
	struct IBEntry
	{
		ComPtr<ID3D12Resource> res; // PlacedResource
		DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
		UINT refCount = 0;
		UINT64 heapOffset = UINT64_MAX;
		std::string debugName = "IBEntry";
		bool bAlive = false;
	};
	struct ObjectEntry
	{
		UINT vbID = 0;
		UINT ibID = 0;
		UINT refCount = 0;
		std::string debugName = "ObjectEntry";
		bool bAlive = false;
	};

	std::vector<VBEntry> m_vbEntries;
	std::vector<IBEntry> m_ibEntries;
	std::vector<ObjectEntry> m_objectEntries;

	UINT64 m_vbCapacity = 0;
	UINT64 m_ibCapacity = 0;

	std::vector<BufferBlock> m_vbFreeList;
	std::vector<BufferBlock> m_vbAllocatedList;
	std::vector<BufferBlock> m_ibFreeList;
	std::vector<BufferBlock> m_ibAllocatedList;
};

