#include "pch.h"
#include "DescriptorAllocator.h"
#include "Core/Engine/EngineCore.h"

DescriptorAllocator::DescriptorAllocator(uint32_t ringCount, uint32_t rtvCount, uint32_t dsvCount, uint32_t samplerCount, uint32_t staticCount, uint32_t descriptorsPerFrame)
{
    ID3D12Device* device = EngineCore::GetDevice();

    // RTV
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = rtvCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    };
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(m_rtvHeap.ReleaseAndGetAddressOf())));
    NAME_D3D12_OBJECT_ALIAS(m_rtvHeap, L"Main_RTV_Heap");

    m_rtvCpuBase = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_rtvGpuBase = m_rtvHeap->GetGPUDescriptorHandleForHeapStart();
    m_rtvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_rtvCount = rtvCount;
    m_nextRtv = 0;

    // DSV
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = dsvCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    };
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(m_dsvHeap.ReleaseAndGetAddressOf())));
    NAME_D3D12_OBJECT_ALIAS(m_dsvHeap, L"Main_DSV_Heap");

    m_dsvCpuBase = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_dsvGpuBase = m_dsvHeap->GetGPUDescriptorHandleForHeapStart();
    m_dsvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_dsvCount = dsvCount;
    m_nextDsv = 0;

	// Sampler
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	desc.NumDescriptors = samplerCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_samplerHeap.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT_ALIAS(m_samplerHeap, L"Sampler");

	m_samplerCpuBase = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
	m_samplerGpuBase = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
	m_samplerInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_samplerCount = samplerCount;
	m_nextSampler = 0;

	// CBV_SRV_UAV
	m_ring = std::make_unique<DescriptorRing>(ringCount, descriptorsPerFrame, staticCount);
	m_cursor.resize(ringCount, 0);
}

uint32_t DescriptorAllocator::AllocateRTV()
{
    ID3D12Device* device = EngineCore::GetDevice();
    assert(device && "Invalid Device");
    assert(m_nextRtv < m_rtvCount && "RTV Descriptor Heap Capacity Exceeded!");

    return m_nextRtv++;

	/*D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NumDescriptors = 1;
	desc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> heap;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
	m_rtvHeaps.push_back(heap);
	return static_cast<uint32_t>(m_rtvHeaps.size() - 1);*/
}

uint32_t DescriptorAllocator::AllocateDSV()
{
    ID3D12Device* device = EngineCore::GetDevice();
    assert(device && "Invalid Device");
    assert(m_nextDsv < m_dsvCount && "DSV Descriptor Heap Capacity Exceeded!");
    return m_nextDsv++;

	/*D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NumDescriptors = 1;
	desc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> heap;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
	m_dsvHeaps.push_back(heap);

	return static_cast<uint32_t>(m_dsvHeaps.size() - 1);*/
}

uint32_t DescriptorAllocator::AllocateSampler()
{
	assert(m_nextSampler < m_samplerCount);
	return m_nextSampler++;
}

uint32_t DescriptorAllocator::AllocateDynamic(uint32_t frameIdx)
{
	assert(frameIdx < m_cursor.size());
	uint32_t& cursor = m_cursor[frameIdx];
	assert(cursor < m_ring->GetSlotsPerFrame());
	return cursor++;
}

uint32_t DescriptorAllocator::AllocateStaticSlot()
{
	assert(m_nextStatic < m_ring->GetStaticCount());
	return m_nextStatic++;
}


void DescriptorAllocator::ResetDynamicSlots(uint32_t frameIdx)
{
	assert(frameIdx < m_cursor.size());
	m_cursor[frameIdx] = 0;
}

void DescriptorAllocator::CreateSRV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};
    d.Format = format;
    d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    auto desc = res->GetDesc();
    d.Texture3D.MipLevels = 1;
    d.Texture3D.MostDetailedMip = 0;
    d.Texture3D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(res, &d, dstCPU);
}

void DescriptorAllocator::CreateUAV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
    d.Format = DXGI_FORMAT_R32_FLOAT;
    d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    d.Texture3D.MipSlice = 0;
    d.Texture3D.FirstWSlice = 0;
    d.Texture3D.WSize = res->GetDesc().DepthOrArraySize;
    device->CreateUnorderedAccessView(res, counter, &d, dstCPU);
}

void DescriptorAllocator::CreateSRV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU)
{
    auto desc = res->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};
    d.Format = DXGI_FORMAT_UNKNOWN;
    d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    d.Buffer.FirstElement = 0;
    d.Buffer.NumElements = uint32_t(desc.Width / stride);
    d.Buffer.StructureByteStride = stride;
    d.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(res, &d, dstCPU);
}

void DescriptorAllocator::CreateUAV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter)
{
    auto desc = res->GetDesc();
    D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
    d.Format = DXGI_FORMAT_UNKNOWN;
    d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    d.Buffer.FirstElement = 0;
    d.Buffer.NumElements = uint32_t(desc.Width / stride);
    d.Buffer.StructureByteStride = stride;
    d.Buffer.CounterOffsetInBytes = 0;
    d.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(res, counter, &d, dstCPU);

}

void DescriptorAllocator::CreateUAV_Raw(ID3D12Device* device, ID3D12Resource* res, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, uint32_t firstElement, uint32_t numElements)
{
    auto desc = res->GetDesc();
    if ((desc.Width % 4ull) != 0ull)
    {
        OutputDebugString(L"Raw Buffer must be 4-byte Aligned!!!!");
    }

    const uint32_t totalElemnts = static_cast<uint32_t>(desc.Width / 4ull);
    if (numElements == 0)
    {
        numElements = totalElemnts - firstElement;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    d.Buffer.FirstElement = firstElement;
    d.Buffer.NumElements = numElements;
    d.Buffer.StructureByteStride = 0;
    d.Buffer.CounterOffsetInBytes = 0;
    d.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

    device->CreateUnorderedAccessView(res, nullptr, &d, dstCPU);

}