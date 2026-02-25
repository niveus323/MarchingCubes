#include "pch.h"
#include "UploadRing.h"

UploadRing::UploadRing(ID3D12Device* device, uint64_t totalSize)
{
	assert(device);
	m_totalSize = totalSize;
	m_head = 0;
	m_tail = m_totalSize;

	D3D12_HEAP_PROPERTIES hpUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(totalSize);

	ThrowIfFailed(device->CreateCommittedResource(&hpUpload, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_uploadBuffer.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_uploadBuffer);

	// 영구 매핑 (https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map)
	D3D12_RANGE readRange = { 0, 0 };
	ThrowIfFailed(m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedPtr)));
}

UploadRing::~UploadRing()
{
	m_mappedPtr = nullptr;
}

// 업로드 할 공간 할당
bool UploadRing::Allocate(const uint64_t size, const uint64_t alignment, uint64_t& outOffset, uint8_t*& outPtr)
{
	uint64_t alignedHead = AlignUp64(m_head, alignment);

	if (m_head < m_tail)
	{
		if (alignedHead + size >= m_tail) return false; //head가 tail과 같아지는 것을 방지하기 위해 >= 사용

		outOffset = alignedHead;
		m_head = alignedHead + size;
	}
	else //m_head >= m_tail
	{
		if (alignedHead + size <= m_totalSize)
		{
			outOffset = alignedHead;
			m_head = alignedHead + size;
		}
		else
		{
			if (size >= m_tail) return false;

			outOffset = 0;
			m_head = size;
		}
	}

	outPtr = m_mappedPtr + outOffset;
	UploadAllocation alloc{};
	alloc.offset = outOffset;
	alloc.size = size;
	alloc.fenceValue = 0;
	m_unframed.push_back(alloc);

	return true;
}

// 이번 프레임에 할당된 공간들에 대해 펜스 값을 설정
void UploadRing::TagFence(uint64_t fenceValue)
{
	if (m_unframed.empty()) return;
	for (auto& alloced : m_unframed)
	{
		alloced.fenceValue = fenceValue;
		m_inFlight.push_back(alloced);
	}
	m_unframed.clear();
}

void UploadRing::Reclaim(uint64_t completedFenceValue)
{
	while (!m_inFlight.empty())
	{
		const auto& front = m_inFlight.front();
		if (front.fenceValue == 0 || front.fenceValue > completedFenceValue) break;

		uint64_t end = front.offset + front.size;
		if (end >= m_totalSize)	end -= m_totalSize; // warp
		m_tail = (end == 0) ? m_totalSize : end;

		m_inFlight.pop_front();
	}
}
