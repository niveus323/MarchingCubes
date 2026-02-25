#include "pch.h"
#include "GpuAllocator.h"
#include "Core/Engine/EngineCore.h"
#include <algorithm>

GpuAllocator::GpuAllocator(uint64_t ringBytes, uint64_t staticPoolBytes, uint64_t dynamicPoolBytes)
{
	auto device = EngineCore::GetDevice();
	assert(device);

	m_ring = std::make_unique<UploadRing>(device, ringBytes);
	m_staticPool = std::make_unique<GPUBufferPool>(device, staticPoolBytes, L"StaticPool");
	m_dynamicPool = std::make_unique<GPUBufferPool>(device, dynamicPoolBytes, L"DynamicPool");
}

bool GpuAllocator::Alloc(const AllocDesc& desc, BufferHandle& outHandle)
{
	auto device = EngineCore::GetDevice();
	assert(device);

	bool result = false;
	switch (desc.mode)
	{
		case AllocDesc::EMode::STATIC:
		case AllocDesc::EMode::DYNAMIC:
		{
			result = AllocFromPool(device, desc, outHandle);
		}
		break;
		case AllocDesc::EMode::RING:
		{
			result = AllocFromRing(device, desc, outHandle);
		}
		break;
		case AllocDesc::EMode::DEDICATED:
		{
			return AllocFromFallback(device, desc, outHandle);
		}
		break;
	}

	if (!result)
	{
		return AllocFromFallback(device, desc, outHandle);
	}

	return true;
}

void GpuAllocator::FreeLater(BufferHandle& handle, uint64_t fence)
{
	handle.retireFence = fence;
	if (handle.res == m_staticPool->GetResource())
	{
		m_staticPool->FreeLater(handle, fence);
	}
	if (handle.res == m_dynamicPool->GetResource())
	{
		m_dynamicPool->FreeLater(handle, fence);
	}
	
}
void GpuAllocator::TagFence(uint64_t fenceValue)
{
	m_ring->TagFence(fenceValue);
	
	for (auto& fallback : m_fallbackUploads)
	{
		if (fallback.refCount)
		{
			fallback.fenceValue = fenceValue;
			fallback.refCount = 0;
		}
	}
}
void GpuAllocator::Reclaim(uint64_t completedFenceValue)
{
	m_lastCompletedFenceValue = completedFenceValue;
	m_ring->Reclaim(completedFenceValue);
	m_staticPool->Reclaim(completedFenceValue);
	m_dynamicPool->Reclaim(completedFenceValue);
	
	for (auto iter = m_fallbackUploads.begin(); iter != m_fallbackUploads.end();)
	{
		if (iter->fenceValue && iter->fenceValue <= completedFenceValue && iter->refCount == 0)
			iter = m_fallbackUploads.erase(iter);
		else
			++iter;
	}

}

#ifdef _DEBUG
std::vector<MemoryInfo> GpuAllocator::GetMemoryInfos() const
{
	std::vector<MemoryInfo> result;
	result.reserve(8);

	auto PushPoolInfo = [&](GPUBufferPool* pool, const std::string& name) {
		if (pool)
		{
			result.push_back(pool->GetPoolInfo(name));
		}
	};

	PushPoolInfo(m_staticPool.get(), "Static");
	PushPoolInfo(m_dynamicPool.get(), "Dynamic");
	
	return result;
}

std::vector<DedicatedBufferInfo> GpuAllocator::GetDebugDedicatedBuffers() const
{
	std::vector<DedicatedBufferInfo> infos;
	infos.reserve(m_fallbackUploads.size()/* + m_promotedResources.size()*/);

	for (const auto& fb : m_fallbackUploads)
	{
		std::string usageStr = "UNKNOWN";
		switch (fb.desc.mode)
		{
			case AllocDesc::EMode::STATIC: usageStr = "Static"; break;
			case AllocDesc::EMode::DYNAMIC: usageStr = "Dynamic"; break;
			case AllocDesc::EMode::DEDICATED: usageStr = "Dedicated"; break;
		}
		
		infos.push_back(DedicatedBufferInfo{
			.type = "Fallback",
			.owner = std::string(fb.owner), // string_view -> string
			.usage = usageStr,
			.size = fb.desc.size, // 혹은 fb.res->GetDesc().Width
			.fenceValue = fb.fenceValue,
			.isLive = (fb.refCount > 0)
			});
	}

	return infos;
}
#endif // _DEBUG

bool GpuAllocator::AllocFromPool(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle)
{
	GPUBufferPool* targetPool = nullptr;
	switch (desc.mode)
	{
		case AllocDesc::EMode::STATIC:
		{
			targetPool = m_staticPool.get();			
		}
		break;
		case AllocDesc::EMode::DYNAMIC:
		{
			targetPool = m_dynamicPool.get();
		}
		break;
		default:
		{
			return false;
		}
		break;
	}

	return targetPool->SubAlloc(device, desc.size, desc.align, outHandle, desc.owner);
}

bool GpuAllocator::AllocFromRing(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle)
{
	uint64_t offset = UINT64_MAX;
	uint8_t* cpuPtr = nullptr;
	if (m_ring->Allocate(desc.size, desc.align, offset, cpuPtr))
	{
		outHandle.res = m_ring->GetResource();
		outHandle.offset = offset;
		outHandle.size = desc.size;
		outHandle.gpuVA = m_ring->GetResource()->GetGPUVirtualAddress() + offset;
		outHandle.cpuPtr = cpuPtr;
		return true;
	}
	return false;
}

bool GpuAllocator::AllocFromFallback(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle)
{
	Log::Print("GpuAllocator", "AllocFromFallback triggered");

	// 이미 생성되어 있는 Fallback 슬롯에서 할당 가능 여부 확인
	int slotIndex = -1;
	for (size_t i = 0; i < m_fallbackUploads.size(); ++i)
	{
		Fallback& fallback = m_fallbackUploads[i];
		if (fallback.refCount != 0 || fallback.fenceValue > m_lastCompletedFenceValue || fallback.desc.heapType != desc.heapType) continue;
		D3D12_HEAP_PROPERTIES hp;
		fallback.res->GetHeapProperties(&hp, nullptr);
		// 힙 타입, 사이즈 체크
		if (hp.Type != desc.heapType || desc.size > fallback.desc.size) continue;

		slotIndex = static_cast<int>(i);
		break;
	}

	if (slotIndex < 0)
	{
		// 없으면 새로 생성
		D3D12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(desc.heapType);
		D3D12_RESOURCE_DESC dc = CD3DX12_RESOURCE_DESC::Buffer(desc.size, D3D12_RESOURCE_FLAGS(desc.flags));
		D3D12_RESOURCE_STATES initial = desc.initialState;
		ComPtr<ID3D12Resource> buffer;
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dc, initial, nullptr, IID_PPV_ARGS(&buffer)));

		Fallback result{
			.desc = desc,
			.res = std::move(buffer),
			.fenceValue = 0
		};
		if (desc.heapType == D3D12_HEAP_TYPE_UPLOAD || desc.heapType == D3D12_HEAP_TYPE_READBACK)
		{
			uint8_t* mapped = nullptr;
			ThrowIfFailed(result.res->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));
			result.ptr = mapped;
		}
		m_fallbackUploads.push_back(std::move(result));
		slotIndex = static_cast<int>(m_fallbackUploads.size() - 1);
		Log::Print("GpuAllocator", "New Fallback Slot Allocated slot = %d", slotIndex);
	}

	// 할당
	auto& fallbackSlot = m_fallbackUploads[slotIndex];
	fallbackSlot.owner = std::string(desc.owner);

	outHandle.res = fallbackSlot.res.Get();
	outHandle.offset = 0;
	outHandle.size = desc.size;
	if (desc.heapType == D3D12_HEAP_TYPE_UPLOAD)
	{
		uint8_t* fallbackptr = fallbackSlot.ptr;
		outHandle.cpuPtr = fallbackSlot.ptr;
	}
	fallbackSlot.refCount = 1;

	return true;
}