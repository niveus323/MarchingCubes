#pragma once
#include "DescriptorRing.h"
#include <queue>
#include <mutex>

class DescriptorAllocator
{
public:
	DescriptorAllocator(uint32_t ringCount = 2u, uint32_t rtvCount = 64u, uint32_t dsvCount = 16u, uint32_t samplerCount = 1, uint32_t staticCount = 2048u, uint32_t descriptorsPerFrame = 128u);
	~DescriptorAllocator() = default;

	DescriptorAllocator(const DescriptorAllocator&) = delete;
	DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;

	uint32_t AllocateRTV();
	uint32_t AllocateDSV();
	uint32_t AllocateSampler();
	uint32_t AllocateDynamic(uint32_t frameIdx);
	uint32_t AllocateStaticSlot(uint32_t count = 1);
	void ResetDynamicSlots(uint32_t frameIdx);
	
	// --- RTV Getters ---
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCpu(uint32_t slot) const
	{
		assert(slot < m_rtvCount);
		D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvCpuBase;
		h.ptr += static_cast<SIZE_T>(slot) * m_rtvInc;
		return h;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetRTVGpu(uint32_t slot) const
	{
		assert(slot < m_rtvCount);
		D3D12_GPU_DESCRIPTOR_HANDLE h = m_rtvGpuBase;
		h.ptr += static_cast<SIZE_T>(slot) * m_rtvInc;
		return h;
	}

	// --- DSV Getters ---
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVCpu(uint32_t slot) const
	{
		assert(slot < m_dsvCount);
		D3D12_CPU_DESCRIPTOR_HANDLE h = m_dsvCpuBase;
		h.ptr += static_cast<SIZE_T>(slot) * m_dsvInc;
		return h;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetDSVGpu(uint32_t slot) const
	{
		assert(slot < m_dsvCount);
		D3D12_GPU_DESCRIPTOR_HANDLE h = m_dsvGpuBase;
		h.ptr += static_cast<UINT64>(slot) * m_dsvInc;
		return h;
	}

	// --- Sampler Getters ---
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

	// --- CBV/SRV/UAV Getters ---
	D3D12_CPU_DESCRIPTOR_HANDLE GetDynamicCpu(uint32_t frameIdx, uint32_t slot) const { return m_ring->CpuAt(frameIdx, slot); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetDynamicGpu(uint32_t frameIdx, uint32_t slot) const { return m_ring->GpuAt(frameIdx, slot); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetStaticCpu(uint32_t slot) const { return m_ring->StaticCpuAt(slot); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetStaticGpu(uint32_t slot) const { return m_ring->StaticGpuAt(slot); }

	// --- Descriptor Heap Getters ---
	ID3D12DescriptorHeap* GetRTVHeap() const { return m_rtvHeap.Get(); }
	ID3D12DescriptorHeap* GetDSVHeap() const { return m_dsvHeap.Get(); }
	ID3D12DescriptorHeap* GetSamplerHeap() const { return m_samplerHeap.Get(); }
	ID3D12DescriptorHeap* GetCbvSrvUavHeap() const { return m_ring->GetHeap(); }

	uint32_t GetRingCount() const { return m_ring->GetRingCount(); }
	uint32_t DescriptorsPerFrame() const { return m_ring->GetSlotsPerFrame(); }

	static void CreateSRV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU);
	static void CreateUAV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter = nullptr);
	static void CreateSRV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU);
	static void CreateUAV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter = nullptr);
	static void CreateUAV_Raw(ID3D12Device* device, ID3D12Resource* res, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, uint32_t firstElement = 0, uint32_t numElements = 0);

private:
	// RTV
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE  m_rtvCpuBase{};
	D3D12_GPU_DESCRIPTOR_HANDLE  m_rtvGpuBase{};
	uint32_t                     m_rtvInc = 0;
	uint32_t                     m_rtvCount = 0;
	uint32_t                     m_nextRtv = 0;

	// DSV
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE  m_dsvCpuBase{};
	D3D12_GPU_DESCRIPTOR_HANDLE  m_dsvGpuBase{};
	uint32_t                     m_dsvInc = 0;
	uint32_t                     m_dsvCount = 0;
	uint32_t                     m_nextDsv = 0;

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

