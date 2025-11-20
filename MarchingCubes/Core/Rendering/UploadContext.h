#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include <functional>

class GpuAllocator;
class StaticBufferRegistry;

class UploadContext
{
public:
	UploadContext(ID3D12Device* device, GpuAllocator* allocator, StaticBufferRegistry* staticBufferRegistry, DescriptorAllocator* descriptorAllocator);
	~UploadContext() = default;

	void Execute(ID3D12GraphicsCommandList* cmdList);
	void TrackPendingAllocations(uint64_t submitFenceValue);
	void Reclaim(uint64_t completedFenceValue);
	void UploadDrawable(IDrawable* drawable, uint64_t completedFenceValue);
	void UploadStatic(IDrawable* drawable, uint64_t completedFenceValue);
	void UploadObjectConstants(uint32_t frameIndex, GeometryBuffer* buf, const ObjectConstants& cb);
	void UploadStructuredBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, uint64_t byteSize, ID3D12Resource* buffer, uint64_t dstOffset, const char* debugName = "");
	void UploadContstants(uint32_t frameIndex, const void* srcData, uint32_t size, BufferHandle& outHandle);
	void UploadTexture2D(ID3D12GraphicsCommandList* cmd, ID3D12Resource* pDestinationResource, const std::vector<D3D12_SUBRESOURCE_DATA>& subResources, const char* debugName = "");

private:
	void EnsureDefaultVB(GeometryBuffer* buf, uint64_t neededSize, const char* debugName = nullptr);
	void EnsureDefaultIB(GeometryBuffer* buf, uint64_t neededSize, const char* debugName = nullptr);
	void FreeBufferHandle(const BufferHandle& handle);
	void UploadTexture2D_Internal(
		ID3D12GraphicsCommandList* cmd, 
		ID3D12Resource* pDestinationResource, 
		const std::vector<D3D12_SUBRESOURCE_DATA>& subResources, 
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after, 
		const char* debugName = "");

private:
	ID3D12Device* m_device = nullptr;
	GpuAllocator* m_allocator = nullptr;
	StaticBufferRegistry* m_staticBufferRegistry = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;

	struct PendingUpload {
		BufferHandle stagingHandle;
		BufferHandle vbHandle;
		BufferHandle ibHandle;
		enum class UploadState : uint8_t {
			Enqueued,
			Recorded,
			InFlight,
			Reclaimed,
			Failed
		} state = UploadState::Enqueued;

		uint64_t vbSize = 0;
		uint64_t ibSize = 0;
		uint64_t vbAligned = 0;
		uint64_t fenceValue = 0;
		IDrawable* drawable = nullptr;
	};
	std::vector<PendingUpload> m_pendingUploads;

	std::vector<BufferHandle> m_reclaimed;
	uint64_t m_lastReclaimedFenceValue = 0;
};

