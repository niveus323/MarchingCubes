#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/PSO/PSOList.h"
#include "Core/Rendering/Material.h"
#include <unordered_set>
#include <unordered_map>

// TODO : Instancing 추가 시 수정.

struct BundleKey
{
	Material* material;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;
	D3D12_PRIMITIVE_TOPOLOGY topology;
	uint32_t indexCount;
};

struct BundleGroup
{
	std::vector<DrawBindingInfo> bindings;
	ID3D12GraphicsCommandList* bundle = nullptr;
	bool bDirty = true;
};

class BundleRecorder
{
	struct Context {
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> bundle;
	};
	struct ContextPool {
		ComPtr<ID3D12PipelineState> pso;
		std::vector<Context> contexts;
		size_t nextIndex = 0;
	};
public:
	BundleRecorder(ID3D12Device* device, ID3D12RootSignature* rootSignature, const PSOList* psoList, size_t contextsPerPSO = 2);
	bool SyncWithPSOList(const PSOList* psoList);
	ID3D12GraphicsCommandList* RecordBundle(std::vector<ID3D12DescriptorHeap*>& heaps, const std::vector<DrawBindingInfo>& bindings, int psoIndex);

private:
	ID3D12Device* m_device = nullptr;
	ID3D12RootSignature* m_rootSignature = nullptr;
	const PSOList* m_psoList = nullptr;
	size_t m_contextsPerPSO = 2;

	std::vector<ContextPool> m_pools;
};

