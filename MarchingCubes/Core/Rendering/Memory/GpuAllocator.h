#pragma once
#include "UploadRing.h"
#include "GPUBufferPool.h"
#include <string_view>

struct AllocDesc
{
	enum class Kind
	{
		DEFAULT = 0,
		CB,
		STAGING,
		READBACK,
		STRUCTURED_UAV
	} kind = Kind::STAGING;
	
	enum class Usage
	{
		GENERIC,
		VERTEX,
		INDEX
	} usage = Usage::GENERIC;

	enum class LifeTime
	{
		SHORT = 0,
		LONG
	}lifetime = LifeTime::SHORT;

	uint64_t size = 0;
	uint32_t align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	std::string_view owner = "";
};

namespace GPUAllocDesc
{
	static inline AllocDesc MakeVertexBufferDesc(uint64_t size, AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::LONG, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::DEFAULT,
			.usage = AllocDesc::Usage::VERTEX,
			.lifetime = lifetime,
			.size = size,
			.owner = ownerName
		};
	}

	static inline AllocDesc MakeIndexBufferDesc(uint64_t size, AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::LONG, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::DEFAULT,
			.usage = AllocDesc::Usage::INDEX,
			.lifetime = lifetime,
			.size = size,
			.owner = ownerName
		};
	}

	static inline AllocDesc MakeGenericBufferDesc(uint64_t size, AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::LONG, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::DEFAULT,
			.usage = AllocDesc::Usage::GENERIC,
			.lifetime = lifetime,
			.size = size,
			.owner = ownerName
		};
	}

	static inline AllocDesc MakeConstantBufferDesc(uint64_t size, AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::SHORT, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::CB,
			.lifetime = lifetime,
			.size = size,
			.align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT,
			.owner = ownerName
		};
	}

	static inline AllocDesc MakeStagingBufferDesc(uint64_t size, uint32_t align,AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::SHORT, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::STAGING,
			.lifetime = lifetime,
			.size = size,
			.align = align,
			.owner = ownerName
		};
	}

	static inline AllocDesc MakeReadbackBufferDesc(uint64_t size, uint32_t align, AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::SHORT, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::READBACK,
			.lifetime = lifetime,
			.size = size,
			.align = align,
			.owner = ownerName
		};
	}

	static inline AllocDesc MakeStructuredUAVDesc(uint64_t size, uint32_t align, AllocDesc::LifeTime lifetime = AllocDesc::LifeTime::LONG, std::string_view ownerName = "")
	{
		return AllocDesc{
			.kind = AllocDesc::Kind::STRUCTURED_UAV,
			.lifetime = lifetime,
			.size = size,
			.align = align,
			.owner = ownerName
		};
	}
}

struct GpuAllocatorInitInfo
{
	uint64_t cbRingBytes = 2ull << 20;
	uint64_t stagingRingBytes = 64ull << 20;
	uint64_t readbackRingBytes = 64ull << 20;
	uint64_t uavRingBytes = 64ull << 20;
	uint64_t vbPoolBytes = 32ull << 20;
	uint64_t ibPoolBytes = 16ull << 20;
	uint64_t genericPoolBytes = 32ull << 20;
};

class GpuAllocator
{
public:
	explicit GpuAllocator(ID3D12Device* device, GpuAllocatorInitInfo info);
	GpuAllocator(ID3D12Device* device, uint64_t cbRingBytes = 2ull << 20, uint64_t stagingRingBytes = 32ull << 20, uint64_t vbPoolBytes = 32ull << 20, uint64_t ibPoolBytes = 16ull << 20);
	~GpuAllocator() = default;

	void Alloc(ID3D12Device* device, const AllocDesc & desc, BufferHandle& outHandle);
	void FreeLater(BufferHandle& handle, uint64_t fence);
	void TagFence(uint64_t fenceValue);
	void Reclaim(uint64_t completedFenceValue);

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
	void AllocFromFallback(ID3D12Device* device, const AllocDesc& desc, BufferHandle& outHandle);

private:
	std::unique_ptr<UploadRing> m_cbRing;
	std::unique_ptr<UploadRing> m_stagingRing;
	std::unique_ptr<GPUBufferPool> m_vbPool;
	std::unique_ptr<GPUBufferPool> m_vbPool_Large;
	std::unique_ptr<GPUBufferPool> m_ibPool;
	std::unique_ptr<GPUBufferPool> m_ibPool_Large;
	std::unique_ptr<GPUBufferPool> m_genericPool;
	std::unique_ptr<GPUBufferPool> m_genericPool_Large;

	// Fallback
	struct Fallback
	{
		AllocDesc desc;
		ComPtr<ID3D12Resource> res;
		uint8_t* ptr = nullptr;
		uint64_t fenceValue = 0;
		uint32_t refCount = 0;
	};
	std::vector<Fallback> m_fallbackUploads;

	// Promoted Default Resource
	struct PromotedResource
	{
		ComPtr<ID3D12Resource> res;
		uint64_t offset = 0;
		uint64_t size = 0;
		uint64_t fenceValue = 0;
		uint64_t refCount = 0;
	};
	std::vector<PromotedResource> m_promotedResources;
	uint64_t m_lastCompletedFenceValue = 0;
};