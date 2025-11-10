#pragma once
#include <deque>
#include <d3d12.h>

struct UploadAllocation {
    UINT64 offset;
    UINT64 size;
    UINT64 fenceValue; // GPU fence value after which this allocation is reclaimable
};

class UploadRing
{
public:
    UploadRing(ID3D12Device* device, UINT64 totalSize, UINT64 align = 256ull);
    ~UploadRing();

    uint8_t* Allocate(UINT64 size, UINT64& outOffset, UINT64 align = 0);
    void TrackAllocation(UINT64 offset, UINT64 size, UINT64 fenceValue);
    void TagFence(UINT64 fenceValue);
    void ReclaimCompleted(UINT64 completedFenceValue);

    ID3D12Resource* GetResource() const { return m_uploadBuffer.Get(); }
    UINT64 GetSize() const { return m_totalSize; }
    uint8_t* GetMappedPtr() const { return m_mappedPtr; }

private:
    ComPtr<ID3D12Resource> m_uploadBuffer;
    uint8_t* m_mappedPtr = nullptr;
    UINT64 m_totalSize = 0;
    UINT64 m_head = 0;
    UINT64 m_tail = 0;
    UINT64 m_align = 256ull;
    std::deque<UploadAllocation> m_inFlight; // 완료 대기 중인 공간들
    std::vector<UploadAllocation> m_unframed; // 새로 할당되어 fence 값이 지정되지 않은 공간들
};

