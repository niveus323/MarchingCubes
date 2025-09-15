#include "pch.h"
#include "BundleRecorder.h"

BundleRecorder::BundleRecorder(ID3D12Device* device, ID3D12RootSignature* rootSignature, const std::unordered_map<PipelineMode, ComPtr<ID3D12PipelineState>>& psos, size_t contextsPerPso)
	: m_device(device), m_rootSignature(rootSignature)
{
	for (auto& pso : psos)
	{
		ContextPool pool;
		pool.pso = pso.second;
		NAME_D3D12_OBJECT_ALIAS(pool.pso, ToLPCWSTR(pso.first));
		pool.nextIndex = 0;
		pool.contexts.resize(contextsPerPso);

		for (Context& context : pool.contexts)
		{
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&context.allocator)));
			NAME_D3D12_OBJECT_ALIAS(context.allocator, ToLPCWSTR(pso.first));
			ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, context.allocator.Get(), pso.second.Get(), IID_PPV_ARGS(&context.bundle)));
			NAME_D3D12_OBJECT_ALIAS(context.bundle, ToLPCWSTR(pso.first));
			context.bundle->SetGraphicsRootSignature(m_rootSignature);
			context.bundle->Close();
		}
		m_pools.push_back(std::move(pool));
	}
}

StaticRenderItem BundleRecorder::CreateBundleFor(const std::vector<IDrawable*>& drawables, PipelineMode mode)
{
	StaticRenderItem item;

	ContextPool& pool = m_pools[size_t(mode)];

	size_t& idx = pool.nextIndex;
	Context& context = pool.contexts[idx];
	idx = (idx + 1) % pool.contexts.size();

	context.allocator->Reset();
	context.bundle->Reset(context.allocator.Get(), pool.pso.Get());
	
	for (IDrawable* drawable : drawables)
	{
		drawable->Draw(context.bundle.Get());
	}
	
	// End Bundle Recording
	context.bundle->Close();
	item.bundle = context.bundle.Get();

	return item;
}
