#pragma once
#include "UploadRing.h"
#include "GPUBufferPool.h"
#include "Core/DataStructures/Data.h"

struct AllocDesc
{
	enum class Kind
	{
		VB = 0,
		IB,
		CB,
		Staging
	} kind = Kind::Staging;
	
	enum class LifeTime
	{
		SHORT = 0,
		LONG
	}lifetime = LifeTime::SHORT;

	UINT64 size = 0;
	UINT64 align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	const char* owner = "";
};

class GpuAllocator
{
public:
	GpuAllocator(ID3D12Device* device, UINT64 cbRingBytes = 2ull << 20, UINT64 stagingRingBytes = 32ull << 20, UINT64 vbPoolBytes = 32ull << 20, UINT64 ibPoolBytes = 16ull << 20);
	~GpuAllocator() = default;

	void Alloc(ID3D12Device* device, const AllocDesc & desc, ResourceSlice& outSlice);
	void FreeLater(ResourceSlice& slice, UINT64 fence);
	void TagFence(UINT64 fenceValue);
	void Reclaim(UINT64 completedFenceValue);

#ifdef _DEBUG
	struct PoolInfo {
		GPUBufferPool* pool;
		std::string name;
	};
	std::vector<PoolInfo> GetDebugPools() const { 
		return { 
			PoolInfo{m_vbPool.get(), "VBPool_Small"}, 
			PoolInfo{m_vbPool_Large.get(), "VBPool_Large"}, 
			PoolInfo{m_ibPool.get(), "IBPool_Small"}, 
			PoolInfo{m_ibPool_Large.get(), "IBPool_Large"}}; 
	}
#endif // _DEBUG


private:
	void AllocFromFallback(ID3D12Device* device, const AllocDesc& desc, ResourceSlice& outSlice);

private:
	std::unique_ptr<UploadRing> m_cbRing;
	std::unique_ptr<UploadRing> m_stagingRing; // VB/IB 업로드용 Ring 버퍼
	std::unique_ptr<GPUBufferPool> m_vbPool;
	std::unique_ptr<GPUBufferPool> m_vbPool_Large;
	std::unique_ptr<GPUBufferPool> m_ibPool;
	std::unique_ptr<GPUBufferPool> m_ibPool_Large;
	// TODO : 리소스/스트리밍 업로드는 별도의 버퍼를 만들것

	// Fallback
	struct Fallback
	{
		AllocDesc desc;
		ComPtr<ID3D12Resource> res;
		uint8_t* ptr = nullptr;
		UINT64 fenceValue = 0;
		UINT refCount = 0;
	};
	std::vector<Fallback> m_fallbackUploads;

	// Promoted Default Resource
	struct PromotedSlice
	{
		ComPtr<ID3D12Resource> res;
		UINT64 offset = 0;
		UINT64 size = 0;
		UINT64 fenceValue = 0;
		UINT64 refCount = 0;
	};
	std::vector<PromotedSlice> m_promotedResources;
	UINT64 m_lastCompletedFenceValue = 0;
};

