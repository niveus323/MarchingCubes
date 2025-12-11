#include "pch.h"
#include "GPUBufferPool.h"
#include <algorithm>
#include "StaticBufferRegistry.h"

GPUBufferPool::GPUBufferPool(ID3D12Device* device, uint64_t totalSize, const wchar_t* debugName)
{
	assert(device && "GPUBufferPool : Invalid Device!!!!");

	m_capacity = AlignUp64(totalSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

	D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(m_capacity);
	ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_buffer)));
	NAME_D3D12_OBJECT_ALIAS(m_buffer, debugName);

	m_free = { {0, m_capacity} };
	m_retired.clear();
}

GPUBufferPool::~GPUBufferPool()
{
	m_retired.clear();
	m_free.clear();
	m_buffer.Reset();
	m_capacity = 0;
}

// 서브할당 & 복사 헬퍼
bool GPUBufferPool::SubAlloc(ID3D12Device* device, uint64_t bytes, uint64_t align, BufferHandle& out, std::string_view owner)
{
	bytes = AlignUp64(bytes, align);
	for (size_t i = 0; i < m_free.size(); ++i)
	{
		const uint64_t blockOffset = m_free[i].offset;
		const uint64_t blockSize = m_free[i].size;
		const uint64_t offset = AlignUp64(blockOffset, align);
		uint64_t padding = offset - blockOffset;
		if (blockSize < padding) continue; // 오프셋 정렬로 인해 free 공간을 벗어나면 continue.

		uint64_t remain = blockSize - padding; // 실제 할당 가능한 남은 공간
		if (remain < bytes) continue; // 가용공간 확인

		out.res = m_buffer.Get();
		out.offset = offset;
		out.size = bytes;
		out.retireFence = 0;

		m_allocated.push_back(BufferBlock(offset, bytes, owner));

		// free 앞 공간 재설정
		if (padding > 0)
		{
			m_free[i].size = padding;
		}
		else
		{
			m_free.erase(m_free.begin() + i);
			--i;
		}

		// 뒷 공간 추가
		uint64_t tailOffset = offset + bytes;
		uint64_t tailSize = (blockOffset + blockSize) - tailOffset;
		if (tailSize > 0) m_free.push_back(BufferBlock(tailOffset, tailSize));
		MergeFree();
		return true;
	}
	out = {};
	return false;
}

void GPUBufferPool::FreeLater(const BufferHandle& r, uint64_t fence)
{
	if (r.size == 0) return;

	// 방어: 이미 m_free에 있는 오프셋인지 체크 (이미 free라면 무시)
	for (auto& fb : m_free)
	{
		if (fb.offset == r.offset && fb.size == r.size)
		{
			// 이미 free에 등록되어 있다면 중복 free 의심 — 무시
			Log::Print("GPUBufferPool", "FreeLater: double-free ignored (offset=%llu size=%llu)", (unsigned long long)r.offset, (unsigned long long)r.size);
			return;
		}
	}

	// 방어: 이미 retired에 같은 항목이 있는지 체크 (중복 retire 방지)
	for (auto& rb : m_retired)
	{
		if (rb.offset == r.offset && rb.size == r.size)
		{
			// 이미 retired에 존재하면 fence를 최신으로 갱신(더 큰 fence 사용)
			if (rb.fence < fence) rb.fence = fence;
			Log::Print("GPUBufferPool", "FreeLater: same retired block found, updated fence (offset=%llu size=%llu fence=%llu)", (unsigned long long)r.offset, (unsigned long long)r.size, (unsigned long long)rb.fence);
			return;
		}
	}
	m_retired.push_back(RetiredBlock(r.offset, r.size, fence));

	for (auto& iter = m_allocated.begin(); iter != m_allocated.end();)
	{
		if (iter->offset == r.offset) iter = m_allocated.erase(iter);
		else ++iter;
	}
}

void GPUBufferPool::Reclaim(uint64_t completedFence)
{
	bool bErased = false;
	for (auto it = m_retired.begin(); it != m_retired.end();)
	{
		if (it->fence && it->fence <= completedFence)
		{
			m_free.emplace_back(it->offset, it->size);
			it = m_retired.erase(it);
			bErased = true;
		}
		else
		{
			++it;
		}
	}

	if(bErased)	MergeFree();
}

void GPUBufferPool::MergeFree()
{
	if (m_free.empty()) return;
	// offset 순으로 재정렬
	std::sort(m_free.begin(), m_free.end(), [](auto& a, auto& b) {return a.offset < b.offset; });
	std::vector<BufferBlock> result;
	result.reserve(m_free.size());
	auto& current = m_free[0];
	for (size_t i = 1; i < m_free.size(); ++i)
	{
		auto& n = m_free[i];
		if (current.offset + current.size == n.offset)
		{
			current.size += n.size;
		}
		else
		{
			result.push_back(current);
			current = n;
		}
	}
	result.push_back(current);
	m_free.swap(result);
}