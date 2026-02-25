#include "pch.h"
#include "Core/DataStructures/Data.h"
#include "Core/Scene/Class/Entity.h"
#include "Core/Engine/EngineCore.h"
#include "UploadContext.h"
#include "Memory/GpuAllocator.h"
#include <unordered_map>

// 업로드 제출
void UploadContext::Execute(ID3D12GraphicsCommandList* cmd)
{
	auto device = EngineCore::GetDevice();
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(device && "UploadContext::Execute : device is Invalid!!!!");
	assert(gpuAllocator && "UploadContext::Execute : allocator is Invalid!!!!");
	
	// 1단계 : COPY_DEST로 상태 전이 -> 암시적 상태 전이가 이루어지므로 불필요
	/*std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(m_pendingUploads.size());
	for (auto& pending : m_pendingUploads)
	{
		if (pending.state != PendingUpload::UploadState::Enqueued) continue;
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pending.dstHandle.res, pending.afterState, D3D12_RESOURCE_STATE_COPY_DEST));
	}
	cmd->ResourceBarrier((uint32_t)barriers.size(), barriers.data());*/

	// 2단계 : Upload -> Default 복사
	for (auto& pending : m_pendingUploads)
	{
		if (pending.state != PendingUpload::UploadState::Enqueued) continue;

		if (pending.size && pending.dstHandle.res != nullptr)
		{
			const BufferHandle& staging = pending.srcHandle;
			cmd->CopyBufferRegion(pending.dstHandle.res, pending.dstHandle.offset, staging.res, staging.offset, pending.size);
			pending.state = PendingUpload::UploadState::Recorded;
		}
	}

	// 3단계 : 상태 되돌리기
	std::vector<D3D12_RESOURCE_BARRIER> restoreBarriers;
	restoreBarriers.reserve(m_pendingUploads.size());
	std::vector<ID3D12Resource*> processedResources;
	processedResources.reserve(m_pendingUploads.size());
	
	for (auto& pending : m_pendingUploads)
	{
		if (pending.state != PendingUpload::UploadState::Recorded) continue;
		if (pending.dstHandle.res == nullptr) continue;
		bool isDuplicate = false;
		for (const auto& res : processedResources)
		{
			if (res == pending.dstHandle.res)
			{
				isDuplicate = true;
				break;
			}
		}

		if (!isDuplicate)
		{
			restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pending.dstHandle.res, D3D12_RESOURCE_STATE_COPY_DEST, pending.afterState));
			processedResources.push_back(pending.dstHandle.res);
		}
	}
	if(!restoreBarriers.empty())
		cmd->ResourceBarrier((uint32_t)restoreBarriers.size(), restoreBarriers.data());

}

// GPU 명령 작업이 끝난 것들은 침범할 수 없도록 inflight 처리
void UploadContext::TrackPendingAllocations(uint64_t submitFenceValue)
{
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "TrackPendingAllocations : Invalid GpuAllocator");

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
			gpuAllocator->FreeLater(handle, submitFenceValue);
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

	m_lastReclaimedFenceValue = completedFenceValue;

	auto it = std::remove_if(m_pendingUploads.begin(), m_pendingUploads.end(), [](const PendingUpload& p) { return p.state == PendingUpload::UploadState::Reclaimed; });
	if (it != m_pendingUploads.end())
		m_pendingUploads.erase(it, m_pendingUploads.end());
}

void UploadContext::UploadStructuredBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, uint64_t byteSize, ID3D12Resource* buffer, uint64_t dstOffset, std::string_view debugName)
{
	if (!srcData || !buffer || byteSize == 0) return;
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "UploadStructuredBuffer : Invalid GpuAllocator");

	BufferHandle staging{};
	gpuAllocator->Alloc(AllocDesc::Ring(byteSize, 4), staging);
	assert(staging.cpuPtr != nullptr && "Allocated Buffer Ptr is Invalid!!!!");
	std::memcpy(staging.cpuPtr, srcData, byteSize);

	cmd->CopyBufferRegion(buffer, dstOffset, staging.res, staging.offset, byteSize);
}

void UploadContext::UploadConstants(const void* srcData, uint32_t size, BufferHandle& outHandle)
{
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "UploadConstants : Invalid GpuAllocator");

	gpuAllocator->Alloc(AllocDesc::Ring(size), outHandle);
	assert(outHandle.cpuPtr != nullptr && "Handle Ptr is Invalid!!!!");
	memcpy(outHandle.cpuPtr, srcData, size);
}

// 동적 할당/해제 형태로 두어 업데이트가 필요할 때만 이루어지도록 한다
void UploadContext::UploadDynamicConstants(const void* srcData, uint32_t size, BufferHandle& outHandle, Entity* owner)
{
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "UploadDynamicConstants : Invalid GpuAllocator");

	// 기존 Default 버퍼가 있다면 해제 예약을 걸어 둔다.(N프레임의 작업 내용을 N+1프레임에 수정하는 Data Hazard를 방지하기 위해 새로 할당)
	if (outHandle.res != nullptr) FreeBufferHandle(outHandle);

	gpuAllocator->Alloc(AllocDesc::Dynamic(size, owner->GetName(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT), outHandle);
	assert(outHandle.res != nullptr);

	BufferHandle staging{};
	gpuAllocator->Alloc(AllocDesc::Ring(size), staging);

	assert(staging.cpuPtr != nullptr && "Allocated Buffer Ptr is Invalid!!!!");
	memcpy(staging.cpuPtr, srcData, size);

	PushOrUpdateUpload(outHandle, staging, size, owner);
}

void UploadContext::UploadTexture(
	ID3D12GraphicsCommandList* cmd,
	ID3D12Resource* pDestinationResource,
	const std::vector<D3D12_SUBRESOURCE_DATA>& subResources,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after,
	std::string_view debugName)
{
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "UploadTexture : Invalid GpuAllocator");

	const UINT numSubresources = static_cast<UINT>(subResources.size());
	const UINT64 requiredSize = GetRequiredIntermediateSize(pDestinationResource, 0, numSubresources);

	BufferHandle handle{};
	gpuAllocator->Alloc(AllocDesc::Ring(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT), handle);
	assert(handle.res != nullptr && "UploadTexture2D : Failed to Allocate!!!!");

	if (before != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pDestinationResource, before, D3D12_RESOURCE_STATE_COPY_DEST));
	}
	UpdateSubresources(cmd, pDestinationResource, handle.res, handle.offset, 0, numSubresources, subResources.data());
	cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pDestinationResource, D3D12_RESOURCE_STATE_COPY_DEST, after));
}

void UploadContext::ResetCounterUAV(ID3D12GraphicsCommandList* cmd, ID3D12Resource* counter, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, std::string_view debugName)
{
	if (!counter || !cmd) return;

	EnsureZeroUintUpload();

	if (before != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(counter, before, D3D12_RESOURCE_STATE_COPY_DEST);
		cmd->ResourceBarrier(1, &toCopyDest);
	}

	// 0을 카운터 버퍼로 복사
	cmd->CopyBufferRegion(counter, 0, m_zeroUintUpload.Get(), 0, sizeof(uint32_t));

	auto toAfter = CD3DX12_RESOURCE_BARRIER::Transition(counter, D3D12_RESOURCE_STATE_COPY_DEST, after);
	cmd->ResourceBarrier(1, &toAfter);
}

void UploadContext::UploadGeometry(GeometryBuffer* buffer, const GeometryData& cpuData, std::string_view debugName)
{
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "UploadGeometry : Invalid GpuAllocator");

	const uint64_t vbBytes = cpuData.vertices.size() * sizeof(Vertex);
	const uint64_t ibBytes = cpuData.indices.size() * sizeof(uint32_t);
	
	if (vbBytes > 0)
	{
		BufferHandle stagingVB{};
		gpuAllocator->Alloc(AllocDesc::Ring(vbBytes, 4ull), stagingVB);  // 4byte 정렬
		assert(stagingVB.cpuPtr!=0 && "Staging VB Ptr is Invalid!!!!");
		memcpy(stagingVB.cpuPtr, cpuData.vertices.data(), vbBytes);
		
		PushOrUpdateUpload(buffer->GetVBHandle(), stagingVB, vbBytes, buffer);
	}

	if (ibBytes > 0)
	{
		BufferHandle stagingIB{};
		gpuAllocator->Alloc(AllocDesc::Ring(ibBytes, 4ull), stagingIB); // R32_UINT 사용하므로 4bytes 정렬
		assert(stagingIB.cpuPtr != 0 && "Staging IB Ptr is Invalid!!!!");
		memcpy(stagingIB.cpuPtr, cpuData.indices.data(), ibBytes);

		PushOrUpdateUpload(buffer->GetIBHandle(), stagingIB, ibBytes, buffer);
	}
}

void UploadContext::FreeGeometryBuffer(GeometryBuffer& buffer)
{
	BufferHandle vb = buffer.GetVBHandle();
	if (vb.size > 0 && vb.res != nullptr)
	{
		FreeBufferHandle(vb);
	}

	BufferHandle ib = buffer.GetIBHandle();
	if (ib.size > 0 && ib.res != nullptr)
	{
		FreeBufferHandle(ib);
	}

	buffer.ReleaseGPUResources();
}

void UploadContext::FreeBufferHandle(BufferHandle& handle)
{
	for (auto& r : m_reclaimed)
	{
		if (r.res == handle.res && r.offset == handle.offset && r.size == handle.size) return;
	}
	m_reclaimed.push_back(handle);
	handle = {};
}

void UploadContext::EnsureZeroUintUpload()
{
	if (m_zeroUintUpload) return;
	auto device = EngineCore::GetDevice();

	auto hpUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t));

	ThrowIfFailed(device->CreateCommittedResource(&hpUpload, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_zeroUintUpload)));
	NAME_D3D12_OBJECT(m_zeroUintUpload);

	// 4바이트 0으로 채워두기
	void* p = nullptr;
	D3D12_RANGE range{ 0, 0 }; // write-only
	ThrowIfFailed(m_zeroUintUpload->Map(0, &range, &p));
	*reinterpret_cast<uint32_t*>(p) = 0u;
	m_zeroUintUpload->Unmap(0, nullptr);
}

void UploadContext::PushOrUpdateUpload(const BufferHandle& dst, const BufferHandle& src, uint64_t size, void* owner, D3D12_RESOURCE_STATES afterState)
{
	auto gpuAllocator = EngineCore::GetGpuAllocator();
	assert(gpuAllocator && "PushOrUpdateUpload : Invalid GpuAllocator");

	for (auto& pending : m_pendingUploads)
	{
		// 아직 기록되지 않은(Enqueued) 요청 중에서, 목적지가 같은 것을 찾음
		if (pending.state == PendingUpload::UploadState::Enqueued &&
			pending.dstHandle.res == dst.res &&
			pending.dstHandle.offset == dst.offset)
		{
			// 기존에 대기 중이던 Staging 메모리는 이제 쓸모가 없으므로 반환해야 함 (아직 GPU에 제출되지 않았으므로 Fence=0으로 즉시 회수 가능하거나, 다음 회수 주기까지 대기)
			gpuAllocator->FreeLater(pending.srcHandle, m_lastReclaimedFenceValue);

			// 새로운 데이터로 교체 (덮어쓰기)
			pending.srcHandle = src;
			pending.size = size;
			pending.owner = owner; // 소유자가 바뀌었을 수도 있으니 갱신
			pending.afterState = afterState;

			return;
		}
	}

	PendingUpload pu{
		.srcHandle = src,
		.dstHandle = dst,
		.state = PendingUpload::UploadState::Enqueued,
		.size = size,
		.owner = owner,
		.afterState = afterState
	};
	m_pendingUploads.push_back(std::move(pu));
}
