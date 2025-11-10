#include "pch.h"
#include "GpuAllocator.h"
#include <algorithm>

GpuAllocator::GpuAllocator(ID3D12Device* device, UINT64 cbRingBytes, UINT64 stagingRingBytes, UINT64 vbPoolBytes, UINT64 ibPoolBytes)
{
	m_cbRing = std::make_unique<UploadRing>(device, cbRingBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	m_stagingRing = std::make_unique<UploadRing>(device, stagingRingBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	UINT64 vbSmallSize = vbPoolBytes >> 2;
	UINT64 vbLargeSize = vbPoolBytes - vbSmallSize;
	m_vbPool = std::make_unique<GPUBufferPool>(device, vbSmallSize, L"VBPool_Small");
	m_vbPool_Large = std::make_unique<GPUBufferPool>(device, vbLargeSize, L"VBPool_Large");

	UINT64 ibSmallSize = ibPoolBytes >> 2;
	UINT64 ibLargeSize = ibPoolBytes - ibSmallSize;
	m_ibPool = std::make_unique<GPUBufferPool>(device, ibSmallSize, L"IBPool_Small");
	m_ibPool_Large = std::make_unique<GPUBufferPool>(device, ibLargeSize, L"IBPool_Large");
}

void GpuAllocator::Alloc(ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice)
{
	auto AllocFromPool = [](GPUBufferPool* pool, ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice) {
		if (!pool->SubAlloc(device, desc.size, desc.align, outSlice, desc.owner))
			return false;
		return true;
	};

	auto AllocFromRing = [](UploadRing* ring, ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice) {
		UINT64 offset = UINT64_MAX;
		uint8_t* cpuPtr = ring->Allocate(desc.size, offset, desc.align);
		if (!cpuPtr || offset == UINT64_MAX)
			return false;

		outSlice.res = ring->GetResource();
		outSlice.offset = offset;
		outSlice.size = desc.size;
		outSlice.gpuVA = ring->GetResource()->GetGPUVirtualAddress() + offset;
		outSlice.cpuPtr = cpuPtr;
		return true;
	};

	auto PromoteAndAlloc = [](ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice, std::vector<PromotedSlice>& promotedResources, UINT64 lastCompletedFenceValue) {
		for (auto& promoted : promotedResources)
		{
			if (promoted.refCount != 0 || promoted.fenceValue > lastCompletedFenceValue || promoted.size < desc.size) continue;
			promoted.refCount = 1;

			outSlice.res = promoted.res.Get();
			outSlice.offset = 0;
			outSlice.size = desc.size;
			outSlice.retireFence = 0;
			outSlice.gpuVA = promoted.res->GetGPUVirtualAddress();

			return;
		}

		auto pow2Round = [](UINT64 size) {
			if (size <= 1) return 1ull;
			--size;
			size |= size >> 1;
			size |= size >> 2;
			size |= size >> 4;
			size |= size >> 8;
			size |= size >> 16;
			size |= size >> 32;
			return ++size;
		};

		UINT64 grown = std::max<UINT64>(desc.size, pow2Round(desc.size));
		PromotedSlice slot{};
		D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(grown);
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&slot.res)));
		slot.size = grown;
		slot.refCount = 1;
		promotedResources.push_back(slot);
		
		outSlice.res = slot.res.Get();
		outSlice.offset = 0;
		outSlice.size = desc.size;
		outSlice.retireFence = 0;
		outSlice.gpuVA = slot.res->GetGPUVirtualAddress();

		return;
	};

	switch (desc.kind)
	{
		case AllocDesc::Kind::VB:
		case AllocDesc::Kind::IB:
		{
			const bool isVB = (desc.kind == AllocDesc::Kind::VB);
			const UINT64 promoteMin = isVB ? PROMOTE_VB_MIN : PROMOTE_IB_MIN;

			// 승격 기준을 넘었다면 승격
			if (desc.size >= promoteMin && desc.lifetime == AllocDesc::LifeTime::LONG)
			{
				PromoteAndAlloc(device, desc, outSlice, m_promotedResources, m_lastCompletedFenceValue);
				Log::Print("GpuAllocator", "Promoted. offset = %llu, size = %llu", outSlice.offset, outSlice.size);
				return;
			}

			GPUBufferPool* pool_small = isVB ? m_vbPool.get() : m_ibPool.get();
			GPUBufferPool* pool_large = isVB ? m_vbPool_Large.get() : m_ibPool_Large.get();

			auto tryPool = [](GPUBufferPool* pool, ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice)->bool {
				return pool && pool->SubAlloc(device, desc.size, desc.align, outSlice, desc.owner);
			};

			bool ok = false;
			const UINT64 cutOffMin = isVB ? m_vbPool->GetCapacity() >> 4 : m_ibPool->GetCapacity() >> 4;
			const UINT64 cutOffMax = cutOffMin << 2;
			const UINT cutoff = std::clamp<UINT64>(static_cast<UINT64>(isVB ? 2 * PROMOTE_VB_MIN : 2 * PROMOTE_IB_MIN), cutOffMin, cutOffMax);
			if (desc.size <= cutoff)
			{
				// Small → Large → Promote
				ok = tryPool(pool_small, device, desc, outSlice) || tryPool(pool_large, device, desc, outSlice);
			}
			else 
			{
				// Large → Small → Promote
				ok = tryPool(pool_large, device, desc, outSlice) || tryPool(pool_small, device, desc, outSlice);
			}

			if (!ok) 
			{
				AllocFromFallback(device, desc, outSlice);
			}
			return;
		}
		break;
		case AllocDesc::Kind::CB:
		case AllocDesc::Kind::Staging:
		{
			if (!AllocFromRing((desc.kind == AllocDesc::Kind::CB) ? m_cbRing.get() : m_stagingRing.get(), device, desc, outSlice))
			{
				// Fallback
				AllocFromFallback(device, desc, outSlice);
			}
			return;
		}
		break;
		default: // Do Nothing
		{}
		break;
	}
}

void GpuAllocator::FreeLater(ResourceSlice& slice, UINT64 fence)
{
	slice.retireFence = fence;
	if (slice.res == m_vbPool->GetResource())
	{
		m_vbPool->FreeLater(slice, fence);
	}
	if (slice.res == m_vbPool_Large->GetResource())
	{
		m_vbPool_Large->FreeLater(slice, fence);
	}
	if (slice.res == m_ibPool->GetResource())
	{
		m_ibPool->FreeLater(slice, fence);
	}
	if (slice.res == m_ibPool_Large->GetResource())
	{
		m_ibPool_Large->FreeLater(slice, fence);
	}

	for (auto& promoted : m_promotedResources)
	{
		if (slice.res == promoted.res.Get())
		{
			promoted.fenceValue = fence;
			if (promoted.refCount) --promoted.refCount;
		}
	}
}
void GpuAllocator::TagFence(UINT64 fenceValue)
{
	m_cbRing->TagFence(fenceValue);
	m_stagingRing->TagFence(fenceValue);

	for (auto& fallback : m_fallbackUploads)
	{
		if (fallback.refCount)
		{
			fallback.fenceValue = fenceValue;
			fallback.refCount = 0;
		}
	}
}
void GpuAllocator::Reclaim(UINT64 completedFenceValue)
{
	m_lastCompletedFenceValue = completedFenceValue;
	m_cbRing->ReclaimCompleted(completedFenceValue);
	m_stagingRing->ReclaimCompleted(completedFenceValue);
	m_vbPool->Reclaim(completedFenceValue);
	m_vbPool_Large->Reclaim(completedFenceValue);
	m_ibPool->Reclaim(completedFenceValue);
	m_ibPool_Large->Reclaim(completedFenceValue);

	for (auto iter = m_fallbackUploads.begin(); iter != m_fallbackUploads.end();)
	{
		if (iter->fenceValue && iter->fenceValue <= completedFenceValue && iter->refCount == 0)
			iter = m_fallbackUploads.erase(iter);
		else 
			++iter;
	}

	for (auto iter = m_promotedResources.begin(); iter != m_promotedResources.end();)
	{
		if (iter->fenceValue != 0 && iter->fenceValue <= completedFenceValue)
			iter = m_promotedResources.erase(iter);
		else
			++iter;
	}
}

void GpuAllocator::AllocFromFallback(ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice)
{
	bool bForUpload = (desc.kind == AllocDesc::Kind::Staging || desc.kind == AllocDesc::Kind::CB);
	auto FindRemainSlot = [](std::vector<Fallback>& fallbacks, D3D12_HEAP_TYPE neededType, UINT64 size, UINT64 completedFenceValue) {
		for (size_t i = 0; i < fallbacks.size(); ++i)
		{
			Fallback& fallback = fallbacks[i];
			D3D12_HEAP_PROPERTIES hp;
			fallback.res->GetHeapProperties(&hp, nullptr);
			if (fallback.refCount || hp.Type != neededType || fallback.fenceValue > completedFenceValue || size > fallback.desc.size) continue;

			// fallbackBuffer에 할당 가능하면 이 슬롯을 리턴
			return static_cast<int>(i);
		}
		return -1;
	};

	int slot = FindRemainSlot(m_fallbackUploads, bForUpload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT, desc.size, m_lastCompletedFenceValue);
	if (slot < 0)
	{
		D3D12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(bForUpload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC dc = CD3DX12_RESOURCE_DESC::Buffer(desc.size);
		D3D12_RESOURCE_STATES initial = bForUpload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
		ComPtr<ID3D12Resource> buffer;
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dc, initial, nullptr, IID_PPV_ARGS(&buffer)));

		Fallback result{};
		result.desc = desc;
		if (bForUpload)
		{
			uint8_t* mapped = nullptr;
			ThrowIfFailed(buffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));
			result.ptr = mapped;
		}
		result.res = std::move(buffer);
		result.fenceValue = 0;
		m_fallbackUploads.push_back(std::move(result));

		slot = m_fallbackUploads.size() - 1;

		Log::Print("GpuAllocator", "New Fallback Slot Allocated slot = %d", slot);
	}

	// 할당
	auto& fallbackSlot = m_fallbackUploads[slot];
	outSlice.res = fallbackSlot.res.Get();
	outSlice.offset = 0;
	outSlice.size = desc.size;
	if (bForUpload)
	{
		uint8_t* fallbackptr = fallbackSlot.ptr;
		outSlice.cpuPtr = fallbackSlot.ptr;
	}
	fallbackSlot.refCount = 1;
}
