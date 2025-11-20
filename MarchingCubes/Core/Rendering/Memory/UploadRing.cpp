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
bool UploadRing::Allocate(const uint64_t alignedSize, uint64_t& outOffset, uint8_t*& outPtr)
{
	// 할당 크기가 가능한 전체 크기보다 클 경우 무조건 실패
	if (alignedSize > m_totalSize)
	{
		Log::Print("UploadRing", "Allocated Failed. total : %llu, trying : %llu", m_totalSize, alignedSize);
		return false;
	}

	uint64_t used = (m_head < m_tail) ? (m_totalSize - m_tail) + m_head : m_head - m_tail;
	// 남은 공간에 할당 가능한지 체크
	if (alignedSize > m_totalSize - used - 1)
	{
		Log::Print("UploadRing", "Allocated Failed. remain : %llu, trying : %llu", m_totalSize - used - 1, alignedSize);
		return false;
	}

	if (m_head < m_tail)
	{
		// [head, tail)
		outOffset = m_head;
		m_head += alignedSize;
	}
	else if (alignedSize > m_totalSize - m_head)
	{
		if (alignedSize > m_tail)
		{
			Log::Print("UplaodRing", "Allocated Failed. trying %llu > tail %llu", alignedSize, m_tail);
			return false;
		}

		// [0, tail)
		outOffset = 0;
		m_head = alignedSize;
	}
	else
	{
		// [head, End)
		outOffset = m_head;
		m_head += alignedSize;
	}

	outPtr = m_mappedPtr + outOffset;
	UploadAllocation alloc{};
	alloc.offset = outOffset;
	alloc.size = alignedSize;
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
