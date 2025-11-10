#include "pch.h"
#include "UploadContext.h"
#include "Memory/GpuAllocator.h"
#include "Memory/StaticBufferRegistry.h"

UploadContext::UploadContext(ID3D12Device* device, GpuAllocator* allocator, StaticBufferRegistry* staticBufferRegistry) :
	m_device(device),
	m_allocator(allocator),
	m_staticBufferRegistry(staticBufferRegistry)
{
}

// 업로드 제출
void UploadContext::Execute(ID3D12GraphicsCommandList* cmd)
{
#ifdef _DEBUG
	assert(m_device && "UploadContext::Execute : device is Invalid!!!!");
	assert(m_allocator && "UploadContext::Execute : allocator is Invalid!!!!");
#endif
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
		if (pending.vbSize && pending.vbSlice.res) recordTarget(targets, pending.vbSlice.res, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		if (pending.ibSize && pending.ibSlice.res) recordTarget(targets, pending.ibSlice.res, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}

	if (!targets.empty())
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(targets.size());
		for (auto& [res, back] : targets)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res, back, D3D12_RESOURCE_STATE_COPY_DEST));
		}
		cmd->ResourceBarrier((UINT)barriers.size(), barriers.data());
	}

	// 2단계 : Upload -> Default 복사
	for (auto& pending : m_pendingUploads)
	{
		if (pending.state != PendingUpload::UploadState::Enqueued) continue;

		const ResourceSlice& staging = pending.stagingSlice;
		// vb
		if (pending.vbSize && pending.vbSlice.res)
		{
			cmd->CopyBufferRegion(pending.vbSlice.res, pending.vbSlice.offset, staging.res, staging.offset, pending.vbSize);
		}
		// ib (dst offset = dstIB.offset + vbAligned)
		if (pending.ibSize && pending.ibSlice.res)
		{
			const UINT64 ibOffset = staging.offset + pending.vbAligned;
			cmd->CopyBufferRegion(pending.ibSlice.res, pending.ibSlice.offset, staging.res, ibOffset, pending.ibSize);
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
		cmd->ResourceBarrier((UINT)barriers.size(), barriers.data());
	}
}

// GPU 명령 작업이 끝난 것들은 침범할 수 없도록 inflight 처리
void UploadContext::TrackPendingAllocations(UINT64 submitFenceValue)
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

	// 기존 슬라이스를 교체했다면 Free
	if (!m_reclaimed.empty())
	{
		std::vector<ResourceSlice> toFree;
		toFree.swap(m_reclaimed);
		for (auto& slice : toFree)
		{
			m_allocator->FreeLater(slice, submitFenceValue);
		}
	}
}

// GPU 명령이 끝나면 다음 프레임에 할당 가능하도록 세팅
void UploadContext::ReclaimCompleted(UINT64 completedFenceValue)
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
void UploadContext::UploadDrawable(IDrawable* drawable, UINT64 completedFenceValue)
{
	if (!drawable) return;

	const GeometryData* cpuData = drawable->GetCPUData();
	GeometryBuffer* buf = drawable->GetGPUBuffer();
	if (!cpuData || !buf) return;

	const UINT64 vbBytes = cpuData->vertices.size() * sizeof(Vertex);
	const UINT64 ibBytes = cpuData->indices.size() * sizeof(uint32_t);
	UINT64 vbAligned = AlignUp64(vbBytes, 4ull); // 4byte 정렬
	UINT64 ibAligned = AlignUp64(ibBytes, 4ull); // R32_UINT 사용하므로 4bytes 정렬
	UINT64 totalBytes = vbAligned + ibAligned;

	// Default VB/IB 바인딩되어있는지 확인하고 없으면 할당받는다.
	EnsureDefaultVB(buf, vbBytes, drawable->GetDebugName());
	EnsureDefaultIB(buf, ibBytes, drawable->GetDebugName());

	// Stage VB/IB
	ResourceSlice stagingSlice{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::Staging,
		.size = totalBytes,
		.align = 4ull, 
		.owner = drawable->GetDebugName()
	};
	m_allocator->Alloc(m_device, desc, stagingSlice);

	uint8_t* ptr = stagingSlice.cpuPtr;
#ifdef _DEBUG
	assert(ptr && "Staging Slice Ptr is Invalid !!!!");
#endif // _DEBUG

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
			e.vbSlice = buf->GetCurrentVBSlice();
			e.ibSlice = buf->GetCurrentIBSlice();
			e.stagingSlice = stagingSlice;
			break;
		}
	}
	if (!already)
	{
		PendingUpload pu{};
		pu.stagingSlice = stagingSlice;
		pu.vbSlice = buf->GetCurrentVBSlice();
		pu.ibSlice = buf->GetCurrentIBSlice();
		pu.state = PendingUpload::UploadState::Enqueued;
		pu.vbSize = vbBytes;
		pu.ibSize = ibBytes;
		pu.vbAligned = vbAligned;
		pu.drawable = drawable;
		m_pendingUploads.push_back(std::move(pu));
	}
}

void UploadContext::UploadStatic(IDrawable* drawable, UINT64 completedFenceValue)
{
	if (!drawable) return;

	const GeometryData* cpuData = drawable->GetCPUData();
	GeometryBuffer* buf = drawable->GetGPUBuffer();
	if (!cpuData || !buf) return;

	const UINT64 vbBytes = cpuData->vertices.size() * sizeof(Vertex);
	const UINT64 ibBytes = cpuData->indices.size() * sizeof(uint32_t);
	UINT64 vbAligned = AlignUp64(vbBytes, 4ull); // 4byte 정렬
	UINT64 ibAligned = AlignUp64(ibBytes, 4ull); // R32_UINT 사용하므로 4bytes 정렬
	UINT64 totalBytes = vbAligned + ibAligned;

	// Default VB/IB 바인딩되어있는지 확인하고 없으면 할당받는다.
	ResourceSlice defaultVBSlice{}, defaultIBSlice{};
	m_staticBufferRegistry->CreateStatic(m_device, vbBytes, ibBytes, sizeof(Vertex), DXGI_FORMAT_R32_UINT, &defaultVBSlice, &defaultIBSlice, drawable->GetDebugName());
	// 정적 오브젝트는 초기 업로드에서만 VBSlice, IBSlice를 세팅하므로 oldSlice는 없음
	buf->BindVBSlice(defaultVBSlice);
	buf->BindIBSlice(defaultIBSlice);

	// Stage VB/IB
	ResourceSlice stagingSlice{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::Staging,
		.size = totalBytes,
		.align = 4ull,
		.owner = drawable->GetDebugName()
	};
	m_allocator->Alloc(m_device, desc, stagingSlice);

	uint8_t* ptr = stagingSlice.cpuPtr;
#ifdef _DEBUG
	assert(ptr && "Staging Slice Ptr is Invalid !!!!");
#endif // _DEBUG

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
			e.stagingSlice = stagingSlice;
			e.vbSlice = buf->GetCurrentVBSlice();
			e.ibSlice = buf->GetCurrentIBSlice();
			break;
		}
	}
	if (!already)
	{
		PendingUpload pu{};
		pu.stagingSlice = stagingSlice;
		pu.vbSlice = buf->GetCurrentVBSlice();
		pu.ibSlice = buf->GetCurrentIBSlice();
		pu.state = PendingUpload::UploadState::Enqueued;
		pu.vbSize = vbBytes;
		pu.ibSize = ibBytes;
		pu.vbAligned = vbAligned;
		pu.drawable = drawable;
		m_pendingUploads.push_back(std::move(pu));
	}
}

// Object CB 업로드
void UploadContext::UploadObjectConstants(GeometryBuffer* buf, const ObjectConstants& cb)
{
	if (!buf) return;

	ResourceSlice slice{};
	AllocDesc desc{
		.kind = AllocDesc::Kind::CB,
		.size = AlignUp64(sizeof(ObjectConstants), CB_ALIGN),
		.align = CB_ALIGN
	};
	m_allocator->Alloc(m_device, desc, slice);
#ifdef _DEBUG
	assert(slice.cpuPtr && "CB Slice Ptr is Invalid!!!!");
#endif // _DEBUG
	memcpy(slice.cpuPtr, &cb, sizeof(ObjectConstants));

	ResourceSlice old{};
	buf->BindCBSlice(slice, &old);
	if (old.res)
	{
		bool already = false;
		for (auto& r : m_reclaimed) {
			if (r.res == old.res && r.offset == old.offset && r.size == old.size) { already = true; break; }
		}
		if (!already) m_reclaimed.push_back(old);
	}
}

void UploadContext::EnsureDefaultVB(GeometryBuffer* buf, UINT64 neededSize, const char* debugName)
{
	ResourceSlice curVB = buf->GetCurrentVBSlice();
	if (curVB.size < neededSize)
	{
		ResourceSlice alloc{};
		AllocDesc desc{
			.kind = AllocDesc::Kind::VB,
			.size = neededSize,
			.align = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.owner = debugName
		};
		m_allocator->Alloc(m_device, desc, alloc);

		ResourceSlice old{};
		buf->BindVBSlice(alloc, &old);
		if (old.res)
		{
			bool already = false;
			for (auto& r : m_reclaimed) {
				if (r.res == old.res && r.offset == old.offset && r.size == old.size) { already = true; break; }
			}
			if (!already) m_reclaimed.push_back(old);
		}
		return;
	}
}

void UploadContext::EnsureDefaultIB(GeometryBuffer* buf, UINT64 neededSize, const char* debugName)
{
	ResourceSlice curIB = buf->GetCurrentIBSlice();
	if (curIB.size < neededSize)
	{
		ResourceSlice alloc{};
		AllocDesc desc{
			.kind = AllocDesc::Kind::IB,
			.size = neededSize,
			.align = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.owner = debugName
		};
		m_allocator->Alloc(m_device, desc, alloc);

		ResourceSlice old{};
		buf->BindIBSlice(alloc, &old);
		if (old.res)
		{
			bool already = false;
			for (auto& r : m_reclaimed) {
				if (r.res == old.res && r.offset == old.offset && r.size == old.size) { already = true; break; }
			}
			if (!already) m_reclaimed.push_back(old);
		}
	}
}