#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/DataStructures/ShaderTypes.h"
#include <string_view>
#include <d3d12.h>
#include <cstdint>
#include <vector>
#include "Core/DataStructures/Data.h"

class GpuAllocator;
class Entity;

class UploadContext
{
public:
	void Execute(ID3D12GraphicsCommandList* cmdList);
	void TrackPendingAllocations(uint64_t submitFenceValue);
	void Reclaim(uint64_t completedFenceValue);
	void UploadStructuredBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, uint64_t byteSize, ID3D12Resource* buffer, uint64_t dstOffset, std::string_view debugName = "");
	void UploadConstants(const void* srcData, uint32_t size, BufferHandle& outHandle);
	void UploadDynamicConstants(const void* srcData, uint32_t size, BufferHandle& outHandle, Entity* owner);
	void UploadTexture(ID3D12GraphicsCommandList* cmd, ID3D12Resource* pDestinationResource, const std::vector<D3D12_SUBRESOURCE_DATA>& subResources, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, std::string_view debugName = "");
	void ResetCounterUAV(ID3D12GraphicsCommandList* cmd, ID3D12Resource* counter, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, std::string_view debugName = "CounterReset");

	void UploadGeometry(GeometryBuffer* buffer, const GeometryData& cpuData, std::string_view debugName);
	void FreeGeometryBuffer(GeometryBuffer& buffer); 
	void FreeBufferHandle(BufferHandle& handle);
private:
	void EnsureZeroUintUpload();
	void PushOrUpdateUpload(const BufferHandle& dst, const BufferHandle& src, uint64_t size, void* owner, D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_COMMON);

private:
	ComPtr<ID3D12Resource> m_zeroUintUpload;

	struct PendingUpload {
		BufferHandle srcHandle;
		BufferHandle dstHandle;
		enum class UploadState : uint8_t {
			Enqueued,
			Recorded,
			InFlight,
			Reclaimed,
			Failed
		} state = UploadState::Enqueued;

		uint64_t size = 0;
		uint64_t fenceValue = 0;
		void* owner = nullptr;
		D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_COMMON;
	};
	std::vector<PendingUpload> m_pendingUploads;

	std::vector<BufferHandle> m_reclaimed;
	uint64_t m_lastReclaimedFenceValue = 0;
};

