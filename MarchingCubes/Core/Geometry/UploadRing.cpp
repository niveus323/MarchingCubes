#include "pch.h"
#include "UploadRing.h"

UploadRing::~UploadRing()
{
    if (m_uploadBuffer) 
    {
        m_uploadBuffer->Unmap(0, nullptr);
    }
    m_mappedPtr = nullptr;
}

void UploadRing::Initialize(ID3D12Device* device, UINT64 totalSize)
{
    assert(device);
    m_totalSize = totalSize;
    m_head = 0;

    D3D12_HEAP_PROPERTIES hpUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Alignment = 0;
    rd.Width = totalSize;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(device->CreateCommittedResource(
        &hpUpload,
        D3D12_HEAP_FLAG_NONE,
        &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadBuffer)));
    NAME_D3D12_OBJECT(m_uploadBuffer);

    // 영구 매핑 (https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map)
    D3D12_RANGE readRange = { 0, 0 };
    ThrowIfFailed(m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedPtr)));
}

// 업로드 할 공간 할당
uint8_t* UploadRing::Allocate(UINT64 size, UINT64& outOffset)
{
    const UINT64 alignedSize = AlignUp64(size, kAlignment);

    // 새 할당 요청이 크기를 넘어설 경우
    if (m_head + alignedSize > m_totalSize) {
        UINT64 earliestInFlightStart = UINT64_MAX;
        if (!m_inFlight.empty()) earliestInFlightStart = m_inFlight.front().offset; // front의 위치를 점검하여 전방으로 남는 공간 탐색
        
        if (!m_inFlight.empty() && alignedSize > earliestInFlightStart) {
            // 이미 모든 공간을 사용하였음 -> 할당 불가
            outOffset = UINT64_MAX;
            return nullptr;
        }

        m_head = 0;
    }

    // 새 할당이 기존 공간을 침범하지 않는지 확인
    for (const auto& a : m_inFlight) {
        UINT64 aStart = a.offset;
        UINT64 aEnd = a.offset + a.size;
        UINT64 rStart = m_head;
        UINT64 rEnd = m_head + alignedSize;
        bool overlap = !(rEnd <= aStart || rStart >= aEnd);
        if (overlap) {
            outOffset = UINT64_MAX;
            return nullptr;
        }
    }

    outOffset = m_head;
    uint8_t* cpuPtr = m_mappedPtr + m_head;
    m_head += alignedSize; // 커서 이동
    return cpuPtr; // 할당 가능한 공간의 포인터 주소 반환
}

void UploadRing::TrackAllocation(UINT64 offset, UINT64 size, UINT64 fenceValue)
{
    UploadAllocation a;
    a.offset = offset;
    a.size = size;
    a.fenceValue = fenceValue;
    m_inFlight.push_back(a);
}

void UploadRing::ReclaimCompleted(UINT64 completedFenceValue)
{
    // 완료한 Upload 데이터는 제거
    while (!m_inFlight.empty() && m_inFlight.front().fenceValue <= completedFenceValue) {
        m_inFlight.pop_front();
    }
}
