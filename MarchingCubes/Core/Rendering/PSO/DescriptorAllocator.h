#pragma once
#include "DescriptorRing.h"
#include <queue>
#include <mutex>

struct DescriptorInitInfo
{
	uint32_t ringCount = 2;
	uint32_t samplerCount = 1;
	uint32_t staticCount = 128;
	uint32_t descriptorsPerFrame = 128;
};

class DescriptorAllocator
{
public:
	explicit DescriptorAllocator(ID3D12Device* device, DescriptorInitInfo info);
	DescriptorAllocator(ID3D12Device* device, uint32_t ringCount = 2u, uint32_t samplerCount = 1, uint32_t staticCount = 128u, uint32_t descriptorsPerFrame = 128u);
	~DescriptorAllocator() = default;

	DescriptorAllocator(const DescriptorAllocator&) = delete;
	DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;

	uint32_t AllocateRTV(ID3D12Device* device);
	uint32_t AllocateDSV(ID3D12Device* device);
	uint32_t AllocateSampler();
	uint32_t AllocateDynamic(uint32_t frameIdx);
	uint32_t AllocateStaticSlot();
	void ResetDynamicSlots(uint32_t frameIdx);
	
	D3D12_CPU_DESCRIPTOR_HANDLE GetStaticCpu(uint32_t slot) const { return m_ring->StaticCpuAt(slot); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetStaticGpu(uint32_t slot) const { return m_ring->StaticGpuAt(slot); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDynamicCpu(uint32_t frameIdx, uint32_t slot) const { return m_ring->CpuAt(frameIdx, slot); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetDynamicGpu(uint32_t frameIdx, uint32_t slot) const { return m_ring->GpuAt(frameIdx, slot); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpu(uint32_t slot) const 
	{
		assert(slot < m_samplerCount);
		D3D12_CPU_DESCRIPTOR_HANDLE h = m_samplerCpuBase;
		h.ptr += static_cast<SIZE_T>(slot) * m_samplerInc;
		return h;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpu(uint32_t slot) const
	{
		assert(slot < m_samplerCount);
		D3D12_GPU_DESCRIPTOR_HANDLE h = m_samplerGpuBase;
		h.ptr += static_cast<UINT64>(slot) * m_samplerInc;
		return h;
	}

	ID3D12DescriptorHeap* GetRTVHeap(uint32_t slot) const { return m_rtvHeaps[slot].Get(); }
	ID3D12DescriptorHeap* GetDSVHeap(uint32_t slot) const { return m_dsvHeaps[slot].Get(); }
	ID3D12DescriptorHeap* GetSamplerHeap(uint32_t slot) const { return m_samplerHeap.Get(); }
	ID3D12DescriptorHeap* GetCbvSrvUavHeap() const { return m_ring->GetHeap(); }

	uint32_t GetRingCount() const { return m_ring->GetRingCount(); }
	uint32_t DescriptorsPerFrame() const { return m_ring->GetSlotsPerFrame(); }

private:
	// RTV
	std::vector<ComPtr<ID3D12DescriptorHeap>> m_rtvHeaps;

	// DSV
	std::vector<ComPtr<ID3D12DescriptorHeap>> m_dsvHeaps;

	// SAMPLER
	ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE  m_samplerCpuBase{};
	D3D12_GPU_DESCRIPTOR_HANDLE  m_samplerGpuBase{};
	uint32_t                     m_samplerInc = 0;
	uint32_t                     m_samplerCount = 0;
	uint32_t                     m_nextSampler = 0;

	// CBV_SRV_UAV
	std::unique_ptr<DescriptorRing> m_ring;

	uint32_t m_nextStatic = 0;
	std::vector<uint32_t> m_cursor;
};

