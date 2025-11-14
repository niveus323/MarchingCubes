#include "pch.h"
#include "UploadRing.h"

UploadRing::UploadRing(ID3D12Device* device, uint64_t totalSize, uint64_t align)
{
	assert(device);
	m_totalSize = totalSize;
	m_head = 0;
	m_tail = m_totalSize;
	m_align = align;

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
uint8_t* UploadRing::Allocate(uint64_t size, uint64_t& outOffset, uint64_t align)
{
	uint64_t alignment = align ? align : m_align;
	const uint64_t alignedSize = AlignUp64(size, alignment);

	// 할당 크기가 가능한 전체 크기보다 클 경우 무조건 실패
	if (alignedSize > m_totalSize) {
		outOffset = UINT64_MAX;
		return nullptr;
	}

	// 새 할당 요청이 크기를 넘어설 경우
	if (m_head + alignedSize > m_totalSize)
	{
		if (alignedSize > m_tail)
		{
			outOffset = UINT64_MAX;
			return nullptr;
		}
		m_head = 0;
	}

	// 새 할당이 기존 공간을 침범하지 않는지 확인
	for (const auto& a : m_inFlight)
	{
		uint64_t aStart = a.offset;
		uint64_t aEnd = a.offset + a.size;
		uint64_t rStart = m_head;
		uint64_t rEnd = m_head + alignedSize;
		bool overlap = !(rEnd <= aStart || rStart >= aEnd);
		if (overlap) {
			outOffset = UINT64_MAX;
			return nullptr;
		}
	}

	outOffset = m_head;
	uint8_t* cpuPtr = m_mappedPtr + m_head;
	m_head += alignedSize; // 커서 이동

	UploadAllocation alloc{};
	alloc.offset = outOffset;
	alloc.size = alignedSize;
	alloc.fenceValue = 0;
	m_unframed.push_back(alloc);

	return cpuPtr; // 할당 가능한 공간의 포인터 주소 반환
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
	// 완료한 Upload 데이터는 제거
	while (!m_inFlight.empty() && m_inFlight.front().fenceValue !=0 && m_inFlight.front().fenceValue <= completedFenceValue)
	{
		m_tail = m_inFlight.front().offset + m_inFlight.front().size;
		m_inFlight.pop_front();
	}
}
