#include "pch.h"
#include "UploadContext.h"
#include "Memory/GpuAllocator.h"
#include "Memory/StaticBufferRegistry.h"
#include <unordered_map>

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

// Object CB 업로드
void UploadContext::UploadObjectConstants(uint32_t frameIndex, GeometryBuffer* buf, const ObjectConstants& cb)
{
	if (!buf) return;

	BufferHandle handle{};
	UploadContstants(frameIndex, &cb, sizeof(ObjectConstants), handle);

	buf->SwapCBHandle(handle);
	if (handle.res) FreeBufferHandle(handle);
}

void UploadContext::UploadStructuredBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, uint64_t byteSize, ID3D12Resource* buffer, uint64_t dstOffset, std::string_view debugName)
{
	if (!srcData || !buffer || byteSize == 0) return;
	assert(m_device && m_allocator && "UploadStructuredBuffer : Invalid state");

	BufferHandle staging{};
	m_allocator->Alloc(m_device, GPUAllocDesc::MakeStagingBufferDesc(byteSize, 4, AllocDesc::LifeTime::LONG, debugName), staging);
	assert(staging.cpuPtr != 0 && "Allocated Buffer Ptr is Invalid!!!!");
	std::memcpy(staging.cpuPtr, srcData, byteSize);

	cmd->CopyBufferRegion(buffer, dstOffset, staging.res, staging.offset, byteSize);
}

void UploadContext::UploadContstants(uint32_t frameIndex, const void* srcData, uint32_t size, BufferHandle& outHandle)
{
	m_allocator->Alloc(m_device, GPUAllocDesc::MakeConstantBufferDesc(size), outHandle);
	assert(outHandle.cpuPtr && "Handle Ptr is Invalid!!!!");
	memcpy(outHandle.cpuPtr, srcData, size);
}

void UploadContext::UploadTexture(
	ID3D12GraphicsCommandList* cmd,
	ID3D12Resource* pDestinationResource,
	const std::vector<D3D12_SUBRESOURCE_DATA>& subResources,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after,
	std::string_view debugName)
{
	const UINT numSubresources = static_cast<UINT>(subResources.size());
	const UINT64 requiredSize = GetRequiredIntermediateSize(pDestinationResource, 0, numSubresources);
	BufferHandle handle{};
	m_allocator->Alloc(m_device, GPUAllocDesc::MakeStagingBufferDesc(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, AllocDesc::LifeTime::LONG, debugName), handle);
	assert(handle.res != nullptr && "UploadTexture2D : Failed to Allocate!!!!");
	cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pDestinationResource, before, D3D12_RESOURCE_STATE_COPY_DEST));
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
	const uint64_t vbBytes = cpuData.vertices.size() * sizeof(Vertex);
	const uint64_t ibBytes = cpuData.indices.size() * sizeof(uint32_t);
	uint64_t vbAligned = AlignUp64(vbBytes, 4ull); // 4byte 정렬
	uint64_t ibAligned = AlignUp64(ibBytes, 4ull); // R32_UINT 사용하므로 4bytes 정렬
	uint64_t totalBytes = vbAligned + ibAligned;

	// Default VB/IB 바인딩되어있는지 확인하고 없으면 할당받는다.
	EnsureDefaultVB(buffer, vbBytes, debugName);
	EnsureDefaultIB(buffer, ibBytes, debugName);

	// Stage VB/IB
	BufferHandle stagingHandle{};
	m_allocator->Alloc(m_device, GPUAllocDesc::MakeStagingBufferDesc(totalBytes, 4u, AllocDesc::LifeTime::SHORT, debugName), stagingHandle);

	uint8_t* ptr = stagingHandle.cpuPtr;
	assert(ptr && "Staging Handle Pointer is Invalid !!!!");

	memcpy(ptr, cpuData.vertices.data(), vbBytes);
	memcpy(ptr + vbAligned, cpuData.indices.data(), ibBytes);

	bool already = false;
	for (auto& e : m_pendingUploads)
	{
		// 이미 copy 대기 상태였다면 Upload 버퍼만 교체
		if (e.state == PendingUpload::UploadState::Enqueued && e.buffer == buffer)
		{
			already = true;
			e.vbHandle = buffer->GetVBHandle();
			e.ibHandle = buffer->GetIBHandle();
			e.stagingHandle = stagingHandle;
			break;
		}
	}
	if (!already)
	{
		PendingUpload pu{};
		pu.stagingHandle = stagingHandle;
		pu.vbHandle = buffer->GetVBHandle();
		pu.ibHandle = buffer->GetIBHandle();
		pu.state = PendingUpload::UploadState::Enqueued;
		pu.vbSize = vbBytes;
		pu.ibSize = ibBytes;
		pu.vbAligned = vbAligned;
		pu.buffer = buffer;
		m_pendingUploads.push_back(std::move(pu));
	}
}

void UploadContext::EnsureDefaultVB(GeometryBuffer* buf, uint64_t neededSize, std::string_view debugName)
{
	BufferHandle curVB = buf->GetVBHandle();
	if (curVB.size < neededSize)
	{
		BufferHandle handle{};
		m_allocator->Alloc(m_device, GPUAllocDesc::MakeVertexBufferDesc(neededSize, AllocDesc::LifeTime::LONG, debugName), handle);

		buf->SwapVBHandle(handle);
		if (handle.res) FreeBufferHandle(handle);
		return;
	}
}

void UploadContext::EnsureDefaultIB(GeometryBuffer* buf, uint64_t neededSize, std::string_view debugName)
{
	BufferHandle curIB = buf->GetIBHandle();
	if (curIB.size < neededSize)
	{
		BufferHandle handle{};
		m_allocator->Alloc(m_device, GPUAllocDesc::MakeIndexBufferDesc(neededSize, AllocDesc::LifeTime::LONG, debugName), handle);

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

void UploadContext::EnsureZeroUintUpload()
{
	if (m_zeroUintUpload) return;

	auto hpUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t));

	ThrowIfFailed(m_device->CreateCommittedResource(&hpUpload, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_zeroUintUpload)));
	NAME_D3D12_OBJECT(m_zeroUintUpload);

	// 4바이트 0으로 채워두기
	void* p = nullptr;
	D3D12_RANGE range{ 0, 0 }; // write-only
	ThrowIfFailed(m_zeroUintUpload->Map(0, &range, &p));
	*reinterpret_cast<uint32_t*>(p) = 0u;
	m_zeroUintUpload->Unmap(0, nullptr);
}