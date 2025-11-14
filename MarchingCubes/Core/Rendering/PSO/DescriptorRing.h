#pragma once
class DescriptorRing
{
public:
    DescriptorRing(ID3D12Device* device, uint32_t ringCount, uint32_t descriptorsPerFrame, uint32_t staticCount);

    // µ¿Àû GPU Descriptor Handle À§Ä¡ ÀÎµ¦½Ì
    D3D12_GPU_DESCRIPTOR_HANDLE GpuAt(uint32_t frameIdx, uint32_t slot) const;
    // Á¤Àû GPU Descriptor Handle À§Ä¡ ÀÎµ¦½Ì
    D3D12_GPU_DESCRIPTOR_HANDLE StaticGpuAt(uint32_t index = 0) const;

    // µ¿Àû CPU Descriptor Handle À§Ä¡ ÀÎµ¦½Ì
    D3D12_CPU_DESCRIPTOR_HANDLE CpuAt(uint32_t frameIdx, uint32_t slot) const;
    D3D12_CPU_DESCRIPTOR_HANDLE StaticCpuAt(uint32_t index = 0) const;

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    uint32_t GetIncrement() const { return m_inc; }
    uint32_t GetSlotsPerFrame() const { return m_perFrame; }
    uint32_t GetRingCount() const { return m_ringCount; }
    uint32_t GetStaticCount() const { return m_staticCount; }

    static void CopyToFrameSlot(ID3D12Device* device, DescriptorRing& ring, uint32_t frameIdx, uint32_t slot, D3D12_CPU_DESCRIPTOR_HANDLE srcCPU);
    static void CopyRange(ID3D12Device* device, DescriptorRing& ring, uint32_t frameIdx, uint32_t baseSlot, const D3D12_CPU_DESCRIPTOR_HANDLE* srcCPU, uint32_t count);
    static void SetTable(ID3D12GraphicsCommandList* cmd, DescriptorRing& ring, uint32_t frameIdx, std::initializer_list<std::pair<uint32_t, uint32_t>> paramAndSlots);

private:
    ComPtr<ID3D12DescriptorHeap>  m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_cpuBase{};
    D3D12_GPU_DESCRIPTOR_HANDLE  m_gpuBase{};
    uint32_t m_inc = 0;
    uint32_t m_perFrame = 0;
    uint32_t m_ringCount = 0;
    uint32_t m_staticCount = 0;
};

