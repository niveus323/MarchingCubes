#include "pch.h"
#include "StaticBufferRegistry.h"
#include <algorithm>

StaticBufferRegistry::StaticBufferRegistry(ID3D12Device* device, StaticRegistryInitInfo info):
	m_vbCapacity(info.vbSize),
	m_ibCapacity(info.ibSize)
{
	D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateHeap(&CD3DX12_HEAP_DESC(m_vbCapacity, hp), IID_PPV_ARGS(&m_VBHeap)));
	NAME_D3D12_OBJECT_ALIAS(m_VBHeap, L"StaticVBHeap");
	m_vbFreeList.push_back(BufferBlock(0ull, m_vbCapacity));

	ThrowIfFailed(device->CreateHeap(&CD3DX12_HEAP_DESC(m_ibCapacity, hp), IID_PPV_ARGS(&m_IBHeap)));
	NAME_D3D12_OBJECT_ALIAS(m_IBHeap, L"StaticIBHeap");
	m_ibFreeList.push_back(BufferBlock(0ull, m_ibCapacity));

}

StaticBufferRegistry::StaticBufferRegistry(ID3D12Device* device, uint64_t vbSize, uint64_t ibSize) :
	StaticBufferRegistry(device, StaticRegistryInitInfo{.vbSize = vbSize, .ibSize = ibSize })
{
}

uint32_t StaticBufferRegistry::CreateStatic(ID3D12Device* device, uint32_t vbBytes, uint32_t ibBytes, uint32_t vertexStride, DXGI_FORMAT ibFormat, BufferHandle* outVB, BufferHandle* outIB, std::string_view debugName)
{
	uint32_t vbHandle = CreateStaticVB(device, vbBytes, vertexStride, debugName);
	if (outVB)
	{
		VBEntry& vbEntry = m_vbEntries[vbHandle];
		outVB->res = vbEntry.res.Get();
		wchar_t wname[128]{};
		MultiByteToWideChar(CP_UTF8, 0, debugName.data(), -1, wname, 128);
		NAME_D3D12_OBJECT_ALIAS(vbEntry.res, wname);
		outVB->offset = 0ull;
		outVB->size = vbEntry.res->GetDesc().Width;
		outVB->gpuVA = vbEntry.res->GetGPUVirtualAddress();
	}
	
	uint32_t ibHandle = CreateStaticIB(device, ibBytes, ibFormat, debugName);
	IBEntry& ibEntry = m_ibEntries[ibHandle];
	if (outIB)
	{
		outIB->res = ibEntry.res.Get();
		wchar_t wname[128]{};
		MultiByteToWideChar(CP_UTF8, 0, debugName.data(), -1, wname, 128);
		NAME_D3D12_OBJECT_ALIAS(ibEntry.res, wname);
		outIB->offset = 0ull;
		outIB->size = ibEntry.res->GetDesc().Width;
		outIB->gpuVA = ibEntry.res->GetGPUVirtualAddress();
	}

	ObjectEntry entry{};
	entry.vbID = vbHandle;
	entry.ibID = ibHandle;
	entry.refCount = 1;
	entry.debugName = debugName;
	entry.bAlive = true;
	m_objectEntries.push_back(entry);

	return (uint32_t)(m_objectEntries.size() - 1);
}

void StaticBufferRegistry::Release(uint32_t handle)
{
	if (m_objectEntries.size() >= handle)
	{
		Log::Print("StaticBufferRegistry", "Invalid ObjectEntry Handle!!!!");
		return;
	}
	// ObjectEntry 해제
	ObjectEntry& entry = m_objectEntries[handle];
	if (!entry.bAlive) return;

	if (entry.refCount > 0) --entry.refCount;

	if (entry.refCount == 0)
	{
		if (entry.vbID) ReleaseVB(entry.vbID);

		if (entry.ibID) ReleaseIB(entry.ibID);

		entry = ObjectEntry();
	}
}

uint32_t StaticBufferRegistry::CreateStaticVB(ID3D12Device* device, uint32_t vbBytes, uint32_t vertexStride, std::string_view debugName)
{
	// 사용 가능한 슬롯이 있으면 사용하고 아니면 새로 추가
	uint32_t vbHandle = UINT_MAX;
	for (uint32_t i = 0; i < m_vbEntries.size(); ++i)
	{
		if (!m_vbEntries[i].bAlive)
		{
			vbHandle = i;
			break;
		}
	}

	if (vbHandle == UINT_MAX)
	{
		m_vbEntries.push_back(VBEntry());
		vbHandle = (uint32_t)(m_vbEntries.size() - 1);
	}

	VBEntry& entry = m_vbEntries[vbHandle];
    uint64_t offset = AllocFromHeap(m_vbFreeList, vbBytes);
	// 할당이 실패하면 힙 크기를 동적으로 늘릴 지, 초기 힙 크기를 크게 둘 지 선택할 것.
	assert(offset < UINT64_MAX && "StaticBufferRegistry : VBHeap Overflowed");
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbBytes);
    ThrowIfFailed(device->CreatePlacedResource(m_VBHeap.Get(), offset, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&entry.res)));
    entry.stride = vertexStride;
    entry.heapOffset = offset;
	entry.debugName = debugName;
	entry.refCount = 1;
	entry.bAlive = true;

	m_vbAllocatedList.push_back(BufferBlock(offset, vbBytes, debugName));

    return vbHandle;
}

uint32_t StaticBufferRegistry::CreateStaticIB(ID3D12Device* device, uint32_t ibBytes, DXGI_FORMAT ibFormat, std::string_view debugName)
{
	uint32_t ibHandle = UINT_MAX;
	for (uint32_t i = 0; i < m_ibEntries.size(); ++i)
	{
		if (!m_ibEntries[i].bAlive)
		{
			ibHandle = i;
			break;
		}
	}

	if (ibHandle == UINT_MAX)
	{
		m_ibEntries.push_back(IBEntry());
		ibHandle = (uint32_t)(m_ibEntries.size() - 1);
	}

	IBEntry& entry = m_ibEntries[ibHandle];
	// FreeBlock에서 유효한 Offset을 받아옴
	uint64_t offset = AllocFromHeap(m_ibFreeList, ibBytes);
	// 할당이 실패하면 힙 크기를 동적으로 늘릴 지, 초기 힙 크기를 크게 둘 지 선택할 것.
	assert(offset < UINT64_MAX && "StaticBufferRegistry : IBHeap Overflowed");

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(ibBytes);
	ThrowIfFailed(device->CreatePlacedResource(m_IBHeap.Get(), offset, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&entry.res)));
	entry.format = ibFormat;
	entry.heapOffset = offset;
	entry.debugName = debugName;
	entry.refCount = 1;
	entry.bAlive = true;

	m_ibAllocatedList.push_back(BufferBlock(offset, ibBytes, debugName));

	return ibHandle;
}

uint64_t StaticBufferRegistry::AllocFromHeap(std::vector<BufferBlock>& freeList, uint32_t bytes)
{
	uint64_t alignedBytes = AlignUp64(static_cast<uint64_t>(bytes), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	for (size_t i = 0; i < freeList.size(); ++i)
	{
		const uint64_t blockOffset = freeList[i].offset;
		const uint64_t blockSize = freeList[i].size;
		const uint64_t offset = AlignUp64(blockOffset, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		uint64_t padding = offset - blockOffset;
		if (blockSize < padding) continue; // 오프셋 정렬로 인해 free 공간을 벗어나면 continue.

		uint64_t remain = blockSize - padding; // 실제 할당 가능한 남은 공간
		if (remain < alignedBytes) continue; // 가용공간 확인

		// free 앞 공간 재설정
		if (padding > 0)
		{
			freeList[i].size = padding;
		}
		else
		{
			freeList.erase(freeList.begin() + i);
			--i;
		}

		// 뒷 공간 추가
		uint64_t tailOffset = offset + alignedBytes;
		uint64_t tailSize = (blockOffset + blockSize) - tailOffset;
		if (tailSize > 0) freeList.push_back(BufferBlock(tailOffset, tailSize));
		MergeFree(freeList);
		return offset;
	}
	return UINT64_MAX;
}

void StaticBufferRegistry::ReleaseVB(uint32_t vbHandle)
{
	VBEntry& vbEntry = m_vbEntries[vbHandle];
	if (!vbEntry.bAlive) return;

	if (vbEntry.refCount > 0) --vbEntry.refCount;

	if (vbEntry.refCount == 0)
	{
		if (vbEntry.res)
		{
			const uint64_t placedBytes = vbEntry.res->GetDesc().Width;
			uint64_t offset = vbEntry.heapOffset;
			if (placedBytes && vbEntry.heapOffset < UINT64_MAX)
			{
				Free(m_vbFreeList, m_vbCapacity, vbEntry.heapOffset, placedBytes);

				std::erase_if(m_vbAllocatedList, [offset, placedBytes](BufferBlock& block) { return block.offset == offset && block.size == placedBytes; });
			}
			vbEntry.res.Reset();
		}
		vbEntry = VBEntry();
	}

}

void StaticBufferRegistry::ReleaseIB(uint32_t ibHandle)
{
	IBEntry& ibEntry = m_ibEntries[ibHandle];
	if (!ibEntry.bAlive) return;

	if (ibEntry.refCount > 0) --ibEntry.refCount;

	if (ibEntry.refCount == 0)
	{
		if (ibEntry.res)
		{
			const uint64_t placedBytes = ibEntry.res->GetDesc().Width;
			const uint64_t  offset = ibEntry.heapOffset;
			if (placedBytes && ibEntry.heapOffset < UINT64_MAX)
			{
				Free(m_ibFreeList, m_ibCapacity, ibEntry.heapOffset, placedBytes);
				std::erase_if(m_ibAllocatedList, [offset, placedBytes](BufferBlock& block) { return block.offset == offset && block.size == placedBytes; });
			}
			ibEntry.res.Reset();
		}

		ibEntry = IBEntry();
	}

}

void StaticBufferRegistry::Free(std::vector<BufferBlock>& freeList, uint64_t heapSize, uint64_t offset, uint64_t size)
{
	if (size == 0) return;

	// 경계 & 정렬 보정
	if (offset >= heapSize) return;
	if (offset + size > heapSize) size = heapSize - offset;
	if (size == 0) return;

	uint64_t alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	uint64_t end = offset + size;
	uint64_t alignedOff = (offset / alignment) * alignment;
	uint64_t alignedEnd = ((end + alignment - 1) / alignment) * alignment;
	if (alignedOff > offset) alignedOff = offset;
	if (alignedEnd > heapSize) alignedEnd = heapSize;
	offset = alignedOff;
	size = (alignedEnd > offset) ? (alignedEnd - offset) : 0;
	if (size == 0) return;

	freeList.push_back(BufferBlock(offset, size));
	MergeFree(freeList);
}

void StaticBufferRegistry::MergeFree(std::vector<BufferBlock>& freeList)
{
	if (freeList.empty()) return;
	// offset 순으로 재정렬
	std::sort(freeList.begin(), freeList.end(), [](auto& a, auto& b) {return a.offset < b.offset; });
	std::vector<BufferBlock> result;
	result.reserve(freeList.size());
	auto& current = freeList[0];
	for (size_t i = 1; i < freeList.size(); ++i)
	{
		auto& n = freeList[i];
		if (current.offset + current.size == n.offset)
		{
			current.size += n.size;
		}
		else
		{
			result.push_back(current);
			current = n;
		}
	}
	result.push_back(current);
	freeList.swap(result);
}