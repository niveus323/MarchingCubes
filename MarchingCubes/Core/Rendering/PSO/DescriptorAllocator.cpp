#include "pch.h"
#include "DescriptorAllocator.h"

DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, DescriptorInitInfo info)
{
	//Sampler
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	desc.NumDescriptors = info.samplerCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_samplerHeap.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT_ALIAS(m_samplerHeap, L"Sampler");

	m_samplerCpuBase = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
	m_samplerGpuBase = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
	m_samplerInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_samplerCount = info.samplerCount;
	m_nextSampler = 0;

	// CBV_SRV_UAV
	m_ring = std::make_unique<DescriptorRing>(device, info.ringCount, info.descriptorsPerFrame, info.staticCount);
	m_cursor.resize(info.ringCount, 0);
}

DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, uint32_t ringCount, uint32_t samplerCount, uint32_t staticCount,  uint32_t descriptorsPerFrame) :
	DescriptorAllocator(device, DescriptorInitInfo{ .ringCount = ringCount, .samplerCount = samplerCount, .staticCount = staticCount, .descriptorsPerFrame = descriptorsPerFrame })
{
}

uint32_t DescriptorAllocator::AllocateRTV(ID3D12Device* device)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NumDescriptors = 1;
	desc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> heap;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
	m_rtvHeaps.push_back(heap);
	return static_cast<uint32_t>(m_rtvHeaps.size() - 1);
}

uint32_t DescriptorAllocator::AllocateDSV(ID3D12Device* device)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NumDescriptors = 1;
	desc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> heap;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
	m_dsvHeaps.push_back(heap);

	return static_cast<uint32_t>(m_dsvHeaps.size() - 1);
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
