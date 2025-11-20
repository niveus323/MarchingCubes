#include "pch.h"
#include "DescriptorRing.h"

DescriptorRing::DescriptorRing(ID3D12Device* device, uint32_t ringCount, uint32_t descriptorsPerFrame, uint32_t staticCount) :
    m_ringCount(ringCount),
    m_perFrame(descriptorsPerFrame),
    m_staticCount(staticCount)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = m_staticCount + descriptorsPerFrame * m_ringCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_heap.ReleaseAndGetAddressOf())));
    NAME_D3D12_OBJECT(m_heap);

    m_inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_cpuBase = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_gpuBase = m_heap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorRing::GpuAt(uint32_t frameIdx, uint32_t slot) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = m_gpuBase;
    h.ptr += SIZE_T((m_staticCount + (frameIdx * m_perFrame) + slot) * m_inc);
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorRing::StaticGpuAt(uint32_t index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = m_gpuBase;
    h.ptr += SIZE_T(index * m_inc);
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorRing::CpuAt(uint32_t frameIdx, uint32_t slot) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_cpuBase;
    h.ptr += SIZE_T((m_staticCount + (frameIdx * m_perFrame) + slot) * m_inc);
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorRing::StaticCpuAt(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_cpuBase;
    h.ptr += SIZE_T(index * m_inc);
    return h;
}

void DescriptorRing::CopyToFrameSlot(ID3D12Device* device, DescriptorRing& ring, uint32_t frameIdx, uint32_t slot, D3D12_CPU_DESCRIPTOR_HANDLE srcCPU)
{
    auto dst = ring.CpuAt(frameIdx, slot);
    device->CopyDescriptorsSimple(1, dst, srcCPU, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DescriptorRing::CopyRange(ID3D12Device* device, DescriptorRing& ring, uint32_t frameIdx, uint32_t baseSlot, const D3D12_CPU_DESCRIPTOR_HANDLE* srcCPU, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        CopyToFrameSlot(device, ring, frameIdx, baseSlot + i, srcCPU[i]);
    }
}

void DescriptorRing::SetTable(ID3D12GraphicsCommandList* cmd, DescriptorRing& ring, uint32_t frameIdx, std::initializer_list<std::pair<uint32_t, uint32_t>> paramAndSlots)
{
    for (const auto& item : paramAndSlots)
    {
        uint32_t paramIdx = item.first;
        uint32_t slotIdx = item.second;

        cmd->SetComputeRootDescriptorTable(paramIdx, ring.GpuAt(frameIdx, slotIdx));
    }
}