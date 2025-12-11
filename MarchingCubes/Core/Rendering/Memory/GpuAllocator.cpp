#include "pch.h"
#include "GpuAllocator.h"
#include <algorithm>

GpuAllocator::GpuAllocator(ID3D12Device* device, GpuAllocatorInitInfo info)
{
	m_cbRing = std::make_unique<UploadRing>(device, info.cbRingBytes);
	m_stagingRing = std::make_unique<UploadRing>(device, info.stagingRingBytes);

	uint64_t vbSmallSize = info.vbPoolBytes >> 2;
	uint64_t vbLargeSize = info.vbPoolBytes - vbSmallSize;
	m_vbPool = std::make_unique<GPUBufferPool>(device, vbSmallSize, L"VBPool_Small");
	m_vbPool_Large = std::make_unique<GPUBufferPool>(device, vbLargeSize, L"VBPool_Large");

	uint64_t ibSmallSize = info.ibPoolBytes >> 2;
	uint64_t ibLargeSize = info.ibPoolBytes - ibSmallSize;
	m_ibPool = std::make_unique<GPUBufferPool>(device, ibSmallSize, L"IBPool_Small");
	m_ibPool_Large = std::make_unique<GPUBufferPool>(device, ibLargeSize, L"IBPool_Large");

	uint64_t gbSmallSize = info.genericPoolBytes >> 2;
	uint64_t gbLargeSize = info.genericPoolBytes - gbSmallSize;
	m_genericPool = std::make_unique<GPUBufferPool>(device, gbSmallSize, L"GenericPool_Small");
	m_genericPool_Large = std::make_unique<GPUBufferPool>(device, gbLargeSize, L"GenericPool_Large");
}

GpuAllocator::GpuAllocator(ID3D12Device* device, uint64_t cbRingBytes, uint64_t stagingRingBytes, uint64_t vbPoolBytes, uint64_t ibPoolBytes) :
	GpuAllocator(device, GpuAllocatorInitInfo{ .cbRingBytes = cbRingBytes, .stagingRingBytes = stagingRingBytes, .vbPoolBytes = vbPoolBytes, .ibPoolBytes = ibPoolBytes })
{
}

void GpuAllocator::Alloc(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle)
{
	auto AllocFromPool = [](GPUBufferPool* pool, ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle) {
		if (!pool->SubAlloc(device, desc.size, desc.align, outHandle, desc.owner))
			return false;
		return true;
		};

	auto AllocFromRing = [](UploadRing* ring, ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle) {
		uint64_t offset = UINT64_MAX;
		uint8_t* cpuPtr = nullptr;
		if (!ring->Allocate(AlignUp64(desc.size, desc.align), offset, cpuPtr)) return false;

		outHandle.res = ring->GetResource();
		outHandle.offset = offset;
		outHandle.size = desc.size;
		outHandle.gpuVA = ring->GetResource()->GetGPUVirtualAddress() + offset;
		outHandle.cpuPtr = cpuPtr;
		return true;
		};

	auto PromoteAndAlloc = [](ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle, std::vector<PromotedResource>& promotedResources, uint64_t lastCompletedFenceValue) {
		for (auto& promoted : promotedResources)
		{
			if (promoted.refCount != 0 || promoted.fenceValue > lastCompletedFenceValue || promoted.size < desc.size) continue;
			promoted.refCount = 1;

			outHandle.res = promoted.res.Get();
			outHandle.offset = 0;
			outHandle.size = desc.size;
			outHandle.retireFence = 0;
			outHandle.gpuVA = promoted.res->GetGPUVirtualAddress();

			return;
		}

		auto pow2Round = [](uint64_t size) {
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

		uint64_t grown = std::max<uint64_t>(desc.size, pow2Round(desc.size));
		PromotedResource slot{};
		D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(grown);
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&slot.res)));
		slot.size = grown;
		slot.refCount = 1;
		promotedResources.push_back(slot);

		outHandle.res = slot.res.Get();
		outHandle.offset = 0;
		outHandle.size = desc.size;
		outHandle.retireFence = 0;
		outHandle.gpuVA = slot.res->GetGPUVirtualAddress();

		return;
		};

	switch (desc.kind)
	{
		case AllocDesc::Kind::DEFAULT:
		{
			uint64_t promoteMin = UINT32_MAX;
			GPUBufferPool* pool_small = nullptr;
			GPUBufferPool* pool_large = nullptr;

			switch (desc.usage)
			{
				case AllocDesc::Usage::GENERIC:
				{
					pool_small = m_genericPool.get();
					pool_large = m_genericPool_Large.get();
				}
				break;
				case AllocDesc::Usage::VERTEX:
				{
					promoteMin = PROMOTE_VB_MIN;
					pool_small = m_vbPool.get();
					pool_large = m_vbPool_Large.get();
				}
				break;
				case AllocDesc::Usage::INDEX:
				{
					promoteMin = PROMOTE_IB_MIN;
					pool_small = m_ibPool.get();
					pool_large = m_ibPool_Large.get();
				}
				break;
				default:
				{
					Log::Print("GPUAllocator", "Invalid Aloc Usage");
				}
				return;
			}

			// 승격 기준을 넘었다면 승격
			if (desc.size >= promoteMin && desc.lifetime == AllocDesc::LifeTime::LONG)
			{
				PromoteAndAlloc(device, desc, outHandle, m_promotedResources, m_lastCompletedFenceValue);
				Log::Print("GpuAllocator", "Promoted. offset = %llu, size = %llu", outHandle.offset, outHandle.size);
				return;
			}

			auto tryPool = [](GPUBufferPool* pool, ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle)->bool {
				return pool && pool->SubAlloc(device, desc.size, desc.align, outHandle, desc.owner);
				};

			const uint64_t cutOffMin = pool_small->GetCapacity() >> 4;
			const uint64_t cutOffMax = cutOffMin << 2;
			const uint64_t cutoff = std::clamp<uint64_t>(promoteMin * 2ull, cutOffMin, cutOffMax);
			bool ok = (desc.size > cutoff) ?
				tryPool(pool_large, device, desc, outHandle) || tryPool(pool_small, device, desc, outHandle) :
				tryPool(pool_small, device, desc, outHandle) || tryPool(pool_large, device, desc, outHandle);

			if (!ok)
			{
				AllocFromFallback(device, desc, outHandle);
			}
			return;
		}
		break;
		case AllocDesc::Kind::CB:
		case AllocDesc::Kind::STAGING:
		{
			if (!AllocFromRing((desc.kind == AllocDesc::Kind::CB) ? m_cbRing.get() : m_stagingRing.get(), device, desc, outHandle))
			{
				// Fallback
				AllocFromFallback(device, desc, outHandle);
			}
			return;
		}
		break;
		case AllocDesc::Kind::READBACK:
		case AllocDesc::Kind::STRUCTURED_UAV:
		{
			AllocFromFallback(device, desc, outHandle);
		}
		break;
		default: // Do Nothing
		{}
		break;
	}
}

void GpuAllocator::FreeLater(BufferHandle& handle, uint64_t fence)
{
	handle.retireFence = fence;
	if (handle.res == m_vbPool->GetResource())
	{
		m_vbPool->FreeLater(handle, fence);
	}
	if (handle.res == m_vbPool_Large->GetResource())
	{
		m_vbPool_Large->FreeLater(handle, fence);
	}
	if (handle.res == m_ibPool->GetResource())
	{
		m_ibPool->FreeLater(handle, fence);
	}
	if (handle.res == m_ibPool_Large->GetResource())
	{
		m_ibPool_Large->FreeLater(handle, fence);
	}

	for (auto& promoted : m_promotedResources)
	{
		if (handle.res == promoted.res.Get())
		{
			promoted.fenceValue = fence;
			if (promoted.refCount) --promoted.refCount;
		}
	}
}
void GpuAllocator::TagFence(uint64_t fenceValue)
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
void GpuAllocator::Reclaim(uint64_t completedFenceValue)
{
	m_lastCompletedFenceValue = completedFenceValue;
	m_cbRing->Reclaim(completedFenceValue);
	m_stagingRing->Reclaim(completedFenceValue);
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

void GpuAllocator::AllocFromFallback(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle)
{
	const bool bForUpload = (desc.kind == AllocDesc::Kind::STAGING || desc.kind == AllocDesc::Kind::CB);
	const bool bReadback = (desc.kind == AllocDesc::Kind::READBACK);
	const D3D12_HEAP_TYPE heapType = (bForUpload) ? D3D12_HEAP_TYPE_UPLOAD : (bReadback ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_DEFAULT);

	auto FindRemainSlot = [](std::vector<Fallback>& fallbacks, AllocDesc::Kind kind, D3D12_HEAP_TYPE neededType, uint64_t size, uint64_t completedFenceValue) {
		for (size_t i = 0; i < fallbacks.size(); ++i)
		{
			Fallback& fallback = fallbacks[i];
			if (fallback.refCount != 0 || fallback.fenceValue > completedFenceValue || fallback.desc.kind != kind) continue;
			D3D12_HEAP_PROPERTIES hp;
			fallback.res->GetHeapProperties(&hp, nullptr);

			if (hp.Type != neededType || size > fallback.desc.size) continue;

			// fallbackBuffer에 할당 가능하면 이 슬롯을 리턴
			return static_cast<int>(i);
		}
		return -1;
		};

	int slot = FindRemainSlot(m_fallbackUploads, desc.kind, heapType, desc.size, m_lastCompletedFenceValue);
	if (slot < 0)
	{
		D3D12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(heapType);
		D3D12_RESOURCE_DESC dc = (desc.kind == AllocDesc::Kind::STRUCTURED_UAV)
			? CD3DX12_RESOURCE_DESC::Buffer(desc.size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
			: CD3DX12_RESOURCE_DESC::Buffer(desc.size);
		D3D12_RESOURCE_STATES initial = bForUpload ? D3D12_RESOURCE_STATE_GENERIC_READ : (bReadback ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON);
		ComPtr<ID3D12Resource> buffer;
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dc, initial, nullptr, IID_PPV_ARGS(&buffer)));

		Fallback result{};
		result.desc = desc;
		if (heapType == D3D12_HEAP_TYPE_UPLOAD || heapType == D3D12_HEAP_TYPE_READBACK)
		{
			uint8_t* mapped = nullptr;
			ThrowIfFailed(buffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));
			result.ptr = mapped;
		}
		result.res = std::move(buffer);
		result.fenceValue = 0;
		m_fallbackUploads.push_back(std::move(result));

		slot = static_cast<int>(m_fallbackUploads.size() - 1);

		Log::Print("GpuAllocator", "New Fallback Slot Allocated slot = %d", slot);
	}

	// 할당
	auto& fallbackSlot = m_fallbackUploads[slot];
	outHandle.res = fallbackSlot.res.Get();
	outHandle.offset = 0;
	outHandle.size = desc.size;
	if (bForUpload)
	{
		uint8_t* fallbackptr = fallbackSlot.ptr;
		outHandle.cpuPtr = fallbackSlot.ptr;
	}
	fallbackSlot.refCount = 1;
}
