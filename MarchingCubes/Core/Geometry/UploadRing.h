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
    UploadRing() = default;
    ~UploadRing();

    void Initialize(ID3D12Device* device, UINT64 totalSize);

    // Allocate 'size' bytes from ring. Returns CPU-side pointer and GPU offset.
    // Thread-safety: not thread-safe by default (call from main thread), or add lock if needed.
    // Returns nullptr if allocation cannot be satisfied (caller should call ReclaimCompleted and retry).
    uint8_t* Allocate(UINT64 size, UINT64& outOffset);

    // After a command list that used allocations is submitted, call TrackAllocation()
    // to mark the allocation as in-flight and associated with a fenceValue.
    void TrackAllocation(UINT64 offset, UINT64 size, UINT64 fenceValue);

    // Reclaim allocations whose fenceValue <= completedFence. Call each frame with
    // the graphics/completion fence GetCompletedValue().
    void ReclaimCompleted(UINT64 completedFenceValue);

    ID3D12Resource* GetResource() const { return m_uploadBuffer.Get(); }
    UINT64 GetSize() const { return m_totalSize; }
    uint8_t* GetMappedPtr() const { return m_mappedPtr; }

private:
    static constexpr UINT64 kAlignment = 256;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
    uint8_t* m_mappedPtr = nullptr;
    UINT64 m_totalSize = 0;

    UINT64 m_head = 0;
    std::deque<UploadAllocation> m_inFlight; // 완료 대기 중인 공간들

};

