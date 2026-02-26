#pragma once
#include "UploadRing.h"
#include "GPUBufferPool.h"
#include <string_view>

struct AllocDesc
{
public:
	enum class EMode : uint8_t
	{
		STATIC,		// 정적 할당 (거의 해제되지 않음)
		DYNAMIC,	// 동적 할당 (빈번히 해제됨)
		RING,		// Ring 할당 (매 프레임 해제됨)
		DEDICATED	// 직접 생성 (Fallback)
	} mode = EMode::DEDICATED;

	uint64_t size = 0;
	uint64_t align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	std::string_view owner = "";
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON; // 초기 상태
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT; // DEDICATED 시 세팅
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE; // UAV 등 Flag 설정 시 세팅

public:
	// 동적 할당
	static AllocDesc Dynamic(uint64_t size, std::string_view name = "", uint64_t align = 4)
	{
		return AllocDesc{ 
			.mode = EMode::DYNAMIC, 
			.size = size, 
			.align = align, 
			.owner = name 
		};
	}
	
	// 정적 할당
	static AllocDesc Static(uint64_t size, std::string_view name = "", uint64_t align = 4)
	{
		return AllocDesc{ 
			.mode = EMode::STATIC, 
			.size = size, 
			.align = align, 
			.owner = name 
		};
	}

	// Ring 할당 (Staging 목적)
	static AllocDesc Ring(uint64_t size, uint64_t align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	{
		return AllocDesc{ 
			.mode = EMode::RING, 
			.size = size, 
			.align = align,
			.heapType = D3D12_HEAP_TYPE_UPLOAD
		};
	}

	// 추가 버퍼 할당
	static AllocDesc Dedicated(uint64_t size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, std::string_view name = "")
	{
		return AllocDesc{ 
			.mode = EMode::DEDICATED, 
			.size = size, 
			.owner = name,
			.flags = flags
		};
	}
};

/* [GpuAllocator]
* - LifeTime : Engine Load -> Engine UnLoad
* - OwnerShip : Engine
* - Access : EngineCore::GetGpuAllocator()
* - Responsibility :
*	- Buffer Allocation : GPU 메모리(VRAM)의 생명주기와 할당 방식을 관리
*	- Resource & Heap Management : 리소스 객체/힙을 생성하고 적절한 힙 위치를 결정
*/
class GpuAllocator
{
public:
	GpuAllocator(uint64_t ringBytes = 64ull << 20, uint64_t staticPoolBytes = 128ull << 20, uint64_t dynamicPoolBytes = 128ull << 20);
	~GpuAllocator() = default;

	bool Alloc(const AllocDesc& desc, BufferHandle& outHandle);
	void FreeLater(BufferHandle& handle, uint64_t fence);
	void TagFence(uint64_t fenceValue);
	void Reclaim(uint64_t completedFenceValue);

#ifdef _DEBUG
	std::vector<MemoryInfo> GetMemoryInfos() const;
	std::vector<DedicatedBufferInfo> GetDebugDedicatedBuffers() const;
#endif // _DEBUG


private:
	bool AllocFromPool(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle);
	bool AllocFromRing(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle);
	bool AllocFromFallback(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle);
private:
	std::unique_ptr<UploadRing> m_ring;
	std::unique_ptr<GPUBufferPool> m_staticPool;
	std::unique_ptr<GPUBufferPool> m_dynamicPool;
	
	// Fallback
	struct Fallback
	{
		AllocDesc desc;
		ComPtr<ID3D12Resource> res;
		uint8_t* ptr = nullptr;
		uint64_t fenceValue = 0;
		uint32_t refCount = 0;
		std::string owner = "Fallback";
	};
	std::vector<Fallback> m_fallbackUploads;

	uint64_t m_lastCompletedFenceValue = 0;
};