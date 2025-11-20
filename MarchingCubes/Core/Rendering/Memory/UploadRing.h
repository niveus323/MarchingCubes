#pragma once
#include <deque>
#include <d3d12.h>

struct UploadAllocation {
    uint64_t offset;
    uint64_t size;
    uint64_t fenceValue; // GPU fence value after which this allocation is reclaimable
};

class UploadRing
{
public:
    UploadRing(ID3D12Device* device, uint64_t totalSize);
    ~UploadRing();

    bool Allocate(const uint64_t alignedSize, uint64_t& outOffset, uint8_t*& outPtr);
    void TagFence(uint64_t fenceValue);
    void Reclaim(uint64_t completedFenceValue);

    ID3D12Resource* GetResource() const { return m_uploadBuffer.Get(); }
    uint64_t GetSize() const { return m_totalSize; }
    uint8_t* GetMappedPtr() const { return m_mappedPtr; }

private:
    ComPtr<ID3D12Resource> m_uploadBuffer;
    uint8_t* m_mappedPtr = nullptr;
    uint64_t m_totalSize = 0;
    uint64_t m_head = 0;
    uint64_t m_tail = 0;
    std::deque<UploadAllocation> m_inFlight; // 완료 대기 중인 공간들
    std::vector<UploadAllocation> m_unframed; // 새로 할당되어 fence 값이 지정되지 않은 공간들
};

