#include "pch.h"
#include "UploadContext.h"
#include "Memory/GpuAllocator.h"
#include "Memory/StaticBufferRegistry.h"

UploadContext::UploadContext(ID3D12Device* device, GpuAllocator* allocator, StaticBufferRegistry* staticBufferRegistry, DescriptorAllocator* descriptorAllocator) :
	m_device(device),
	m_allocator(allocator),
	m_staticBufferRegistry(staticBufferRegistry),
	m_descriptorAllocator(descriptorAllocator)
{
}

// 업로드 제출
void UploadContext::Execute(ID3D12GraphicsCommandList* cmd)
{
	assert(m_device && "UploadContext::Execute : device is Invalid!!!!");
	assert(m_allocator && "UploadContext::Execute : allocator is Invalid!!!!");
	std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> targets;
	targets.reserve(m_pendingUploads.size());
	auto recordTarget = [](std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES>& targets, ID3D12Resource* res, D3D12_RESOURCE_STATES back) {
		if (!res) return;
		auto iter = targets.find(res);
		if (iter == targets.end()) targets.emplace(res, back);
		};

	// 1단계 : COPY_DEST로 상태 전이
	for (auto& pending : m_pendingUploads)
	{
		if (pending.state != PendingUpload::UploadState::Enqueued) continue;
		if (pending.vbSize && pending.vbHandle.res) recordTarget(targets, pending.vbHandle.res, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		if (pending.ibSize && pending.ibHandle.res) recordTarget(targets, pending.ibHandle.res, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}

	if (!targets.empty())
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(targets.size());
		for (auto& [res, back] : targets)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res, back, D3D12_RESOURCE_STATE_COPY_DEST));
		}
		cmd->ResourceBarrier((uint32_t)barriers.size(), barriers.data());
	}

	// 2단계 : Upload -> Default 복사
	for (auto& pending : m_pendingUploads)
	{
		if (pending.state != PendingUpload::UploadState::Enqueued) continue;

		const BufferHandle& staging = pending.stagingHandle;
		// vb
		if (pending.vbSize && pending.vbHandle.res)
		{
			cmd->CopyBufferRegion(pending.vbHandle.res, pending.vbHandle.offset, staging.res, staging.offset, pending.vbSize);
		}
		// ib (dst offset = dstIB.offset + vbAligned)
		if (pending.ibSize && pending.ibHandle.res)
		{
			const uint64_t ibOffset = staging.offset + pending.vbAligned;
			cmd->CopyBufferRegion(pending.ibHandle.res, pending.ibHandle.offset, staging.res, ibOffset, pending.ibSize);
		}
		pending.drawable->SetUploadPending(false);
		pending.state = PendingUpload::UploadState::Recorded;
	}

	// 3단계 : 상태 되돌리기
	if (!targets.empty())
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(targets.size());
		for (auto& [res, back] : targets)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res, D3D12_RESOURCE_STATE_COPY_DEST, back));
		}
		cmd->ResourceBarrier((uint32_t)barriers.size(), barriers.data());
	}
}

// GPU 명령 작업이 끝난 것들은 침범할 수 없도록 inflight 처리
void UploadContext::TrackPendingAllocations(uint64_t submitFenceValue)
{
	m_allocator->TagFence(submitFenceValue);

	for (auto& pu : m_pendingUploads)
	{
		if (pu.state == PendingUpload::UploadState::Recorded)
		{
			pu.state = PendingUpload::UploadState::InFlight;
			pu.fenceValue = submitFenceValue;
		}
	}

	// 교체했다면 Free
	if (!m_reclaimed.empty())
	{
		std::vector<BufferHandle> toFree;
		toFree.swap(m_reclaimed);
		for (auto& handle : toFree)
		{
			m_allocator->FreeLater(handle, submitFenceValue);
		}
	}
}

// GPU 명령이 끝나면 다음 프레임에 할당 가능하도록 세팅
void UploadContext::Reclaim(uint64_t completedFenceValue)
{
	for (auto& pu : m_pendingUploads) {
		if (pu.state == PendingUpload::UploadState::InFlight && pu.fenceValue != 0 && pu.fenceValue <= completedFenceValue)
		{
			pu.state = PendingUpload::UploadState::Reclaimed;
		}
	}

	m_allocator->Reclaim(completedFenceValue);
	m_lastReclaimedFenceValue = completedFenceValue;

	auto it = std::remove_if(m_pendingUploads.begin(), m_pendingUploads.end(), [](const PendingUpload& p) { return p.state == PendingUpload::UploadState::Reclaimed; });
	if (it != m_pendingUploads.end())
		m_pendingUploads.erase(it, m_pendingUploads.end());
}

// VB/IB 스테이징
void UploadContext::UploadDrawable(IDrawable* drawable, uint64_t completedFenceValue)
{
	if (!drawable) return;

	const GeometryData* cpuData = drawable->GetCPUData();
	GeometryBuffer* buf = drawable->GetGPUBuffer();
	if (!cpuData || !buf) return;

	const uint32_t vbBytes = static_cast<uint32_t>(cpuData->vertices.size() * sizeof(Vertex));
	const uint32_t ibBytes = static_cast<uint32_t>(cpuData->indices.size() * sizeof(uint32_t));
	uint32_t vbAligned = AlignUp(vbBytes, 4u); // 4byte 정렬
	uint32_t ibAligned = AlignUp(ibBytes, 4u); // R32_UINT 사용하므로 4bytes 정렬
	uint32_t totalBytes = vbAligned + ibAligned;

	// Default VB/IB 바인딩되어있는지 확인하고 없으면 할당받는다.
	EnsureDefaultVB(buf, vbBytes, drawable->GetDebugName());
	EnsureDefaultIB(buf, ibBytes, drawable->GetDebugName());

	// Stage VB/IB
	BufferHandle stagingHandle{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::Staging,
		.size = totalBytes,
		.align = 4ull, 
		.owner = drawable->GetDebugName()
	};
	m_allocator->Alloc(m_device, desc, stagingHandle);

	uint8_t* ptr = stagingHandle.cpuPtr;
	assert(ptr && "Staging Handle Pointer is Invalid !!!!");

	memcpy(ptr, cpuData->vertices.data(), vbBytes);
	memcpy(ptr + vbAligned, cpuData->indices.data(), ibBytes);

	drawable->SetUploadPending(true);

	bool already = false;
	for (auto& e : m_pendingUploads)
	{
		// 이미 copy 대기 상태였다면 Upload 버퍼만 교체
		if (e.state == PendingUpload::UploadState::Enqueued && e.drawable == drawable)
		{
			already = true;
			e.vbHandle = buf->GetCurrentVBHandle();
			e.ibHandle = buf->GetCurrentIBHandle();
			e.stagingHandle = stagingHandle;
			break;
		}
	}
	if (!already)
	{
		PendingUpload pu{};
		pu.stagingHandle = stagingHandle;
		pu.vbHandle = buf->GetCurrentVBHandle();
		pu.ibHandle = buf->GetCurrentIBHandle();
		pu.state = PendingUpload::UploadState::Enqueued;
		pu.vbSize = vbBytes;
		pu.ibSize = ibBytes;
		pu.vbAligned = vbAligned;
		pu.drawable = drawable;
		m_pendingUploads.push_back(std::move(pu));
	}
}

void UploadContext::UploadStatic(IDrawable* drawable, uint64_t completedFenceValue)
{
	if (!drawable) return;

	const GeometryData* cpuData = drawable->GetCPUData();
	GeometryBuffer* buf = drawable->GetGPUBuffer();
	if (!cpuData || !buf) return;

	const uint32_t vbBytes = static_cast<uint32_t>(cpuData->vertices.size() * sizeof(Vertex));
	const uint32_t ibBytes = static_cast<uint32_t>(cpuData->indices.size() * sizeof(uint32_t));
	uint32_t vbAligned = AlignUp(vbBytes, 4u); // 4byte 정렬
	uint32_t ibAligned = AlignUp(ibBytes, 4u); // R32_UINT 사용하므로 4bytes 정렬
	uint32_t totalBytes = vbAligned + ibAligned;

	BufferHandle defaultVBHandle{}, defaultIBHandle{};
	m_staticBufferRegistry->CreateStatic(m_device, vbBytes, ibBytes, sizeof(Vertex), DXGI_FORMAT_R32_UINT, &defaultVBHandle, &defaultIBHandle, drawable->GetDebugName());
	
	buf->SwapVBHandle(defaultVBHandle);
	buf->SwapIBHandle(defaultIBHandle);

	// Stage VB/IB
	BufferHandle stagingHandle{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::Staging,
		.size = totalBytes,
		.align = 4ull,
		.owner = drawable->GetDebugName()
	};
	m_allocator->Alloc(m_device, desc, stagingHandle);

	uint8_t* ptr = stagingHandle.cpuPtr;
	assert(ptr && "Staging Handle Pointer is Invalid !!!!");

	memcpy(ptr, cpuData->vertices.data(), vbBytes);
	memcpy(ptr + vbAligned, cpuData->indices.data(), ibBytes);

	drawable->SetUploadPending(true);

	bool already = false;
	for (auto& e : m_pendingUploads)
	{
		// 이미 copy 대기 상태였다면 Upload 버퍼만 교체
		if (e.state == PendingUpload::UploadState::Enqueued && e.drawable == drawable)
		{
			already = true;
			e.stagingHandle = stagingHandle;
			e.vbHandle = buf->GetCurrentVBHandle();
			e.ibHandle = buf->GetCurrentIBHandle();
			break;
		}
	}
	if (!already)
	{
		PendingUpload pu{};
		pu.stagingHandle = stagingHandle;
		pu.vbHandle = buf->GetCurrentVBHandle();
		pu.ibHandle = buf->GetCurrentIBHandle();
		pu.state = PendingUpload::UploadState::Enqueued;
		pu.vbSize = vbBytes;
		pu.ibSize = ibBytes;
		pu.vbAligned = vbAligned;
		pu.drawable = drawable;
		m_pendingUploads.push_back(std::move(pu));
	}
}

// Object CB 업로드
void UploadContext::UploadObjectConstants(uint32_t frameIndex, GeometryBuffer* buf, const ObjectConstants& cb)
{
	if (!buf) return;

	BufferHandle handle{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::CB,
		.size = AlignUp(sizeof(ObjectConstants), CB_ALIGN),
		.align = CB_ALIGN
	};
	m_allocator->Alloc(m_device, desc, handle);
	assert(handle.cpuPtr && "CB Handle Ptr is Invalid!!!!");
	memcpy(handle.cpuPtr, &cb, sizeof(ObjectConstants));

	buf->SwapCBHandle(handle);
	if (handle.res) FreeBufferHandle(handle);
}

void UploadContext::UploadStructuredBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, uint32_t byteSize, ID3D12Resource* buffer, uint64_t dstOffset, const char* debugName)
{
	if (!srcData || !buffer || byteSize == 0) return;
	assert(m_device && m_allocator && "UploadStructuredBuffer : Invalid state");

	BufferHandle staging{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::Staging,
		.size = byteSize,
		.align = 4,
		.owner = debugName
	};
	m_allocator->Alloc(m_device, desc, staging);
	assert(staging.cpuPtr != 0 && "Allocated Buffer Ptr is Invalid!!!!");
	std::memcpy(staging.cpuPtr, srcData, byteSize);

	cmd->CopyBufferRegion(buffer, dstOffset, staging.res, staging.offset, byteSize);
}

void UploadContext::EnsureDefaultVB(GeometryBuffer* buf, uint32_t neededSize, const char* debugName)
{
	BufferHandle curVB = buf->GetCurrentVBHandle();
	if (curVB.size < neededSize)
	{
		BufferHandle handle{};
		AllocDesc desc{
			.kind = AllocDesc::Kind::VB,
			.size = neededSize,
			.align = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.owner = debugName
		};
		m_allocator->Alloc(m_device, desc, handle);

		buf->SwapVBHandle(handle);
		if (handle.res) FreeBufferHandle(handle);
		return;
	}
}

void UploadContext::EnsureDefaultIB(GeometryBuffer* buf, uint32_t neededSize, const char* debugName)
{
	BufferHandle curIB = buf->GetCurrentIBHandle();
	if (curIB.size < neededSize)
	{
		BufferHandle handle{};
		AllocDesc desc{
			.kind = AllocDesc::Kind::IB,
			.size = neededSize,
			.align = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.owner = debugName
		};
		m_allocator->Alloc(m_device, desc, handle);

		buf->SwapIBHandle(handle);
		if (handle.res) FreeBufferHandle(handle);
	}
}

void UploadContext::FreeBufferHandle(const BufferHandle& handle)
{
	for (auto& r : m_reclaimed) 
	{
		if (r.res == handle.res && r.offset == handle.offset && r.size == handle.size) 
		{ 
			return;
		}
	}
	m_reclaimed.push_back(handle);
}
