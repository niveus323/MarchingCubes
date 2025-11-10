#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include <functional>

struct DynamicRenderItem
{
	IDrawable* object;
};

class GpuAllocator;
class StaticBufferRegistry;

class UploadContext
{
public:
	UploadContext(ID3D12Device* device, GpuAllocator* allocator, StaticBufferRegistry* staticBufferRegistry);
	~UploadContext() = default;

	void Execute(ID3D12GraphicsCommandList* cmdList);
	void TrackPendingAllocations(UINT64 submitFenceValue);
	void ReclaimCompleted(UINT64 completedFenceValue);
	void UploadDrawable(IDrawable* drawable, UINT64 completedFenceValue);
	void UploadStatic(IDrawable* drawable, UINT64 completedFenceValue);
	void UploadObjectConstants(GeometryBuffer* buf, const ObjectConstants& cb);

private:
	void EnsureDefaultVB(GeometryBuffer* buf, UINT64 neededSize, const char* debugName = nullptr);
	void EnsureDefaultIB(GeometryBuffer* buf, UINT64 neededSize, const char* debugName = nullptr);

private:
	ID3D12Device* m_device = nullptr;
	GpuAllocator* m_allocator = nullptr;
	StaticBufferRegistry* m_staticBufferRegistry = nullptr;

	struct PendingUpload {
		ResourceSlice stagingSlice;
		ResourceSlice vbSlice;
		ResourceSlice ibSlice;
		enum class UploadState : uint8_t {
			Enqueued,
			Recorded,
			InFlight,
			Reclaimed,
			Failed
		} state = UploadState::Enqueued;

		UINT64 vbSize = 0;
		UINT64 ibSize = 0;
		UINT64 vbAligned = 0;
		UINT64 fenceValue = 0;
		IDrawable* drawable = nullptr;
	};
	std::vector<PendingUpload> m_pendingUploads;

	std::vector<ResourceSlice> m_reclaimed;
	UINT64 m_lastReclaimedFenceValue = 0;
};

