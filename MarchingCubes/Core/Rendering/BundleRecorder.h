#pragma once
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/PSO/PSOList.h"
#include "Core/Rendering/Material.h"
#include <unordered_set>
#include <unordered_map>

struct StaticRenderItem
{
	ID3D12GraphicsCommandList* bundle = nullptr;
	std::vector<IDrawable*> objects; // 번들에 들어가는 object 리스트
};

struct PendingDeleteItem
{
	UINT64 fenceValue = 0; // 삭제 요청 시점의 fence 값
	std::vector<ComPtr<ID3D12Resource>> resources;
};

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const IDrawable* drawable);

class BundleRecorder
{
public:
	BundleRecorder(ID3D12Device* device, ID3D12RootSignature* rootSignature, const PSOList* psoList, size_t contextsPerPSO = 2);

	ID3D12GraphicsCommandList* CreateBundleFor(const std::vector<IDrawable*>& drawables, const std::string& psoName);
	bool SyncWithPSOList(const PSOList* psoList);
private:
	struct Context {
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> bundle;
	};
	struct ContextPool {
		ComPtr<ID3D12PipelineState> pso;
		std::vector<Context> contexts;
		size_t nextIndex = 0;
	};

	ID3D12Device* m_device = nullptr;
	ID3D12RootSignature* m_rootSignature = nullptr;
	const PSOList* m_psoList = nullptr;
	size_t m_contextsPerPSO = 2;

	std::vector<ContextPool> m_pools;
};

