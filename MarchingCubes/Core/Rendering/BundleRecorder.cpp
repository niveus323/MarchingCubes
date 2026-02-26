#include "pch.h"
#include "BundleRecorder.h"

BundleRecorder::BundleRecorder(ID3D12Device* device, const PSOList* psoList, size_t contextsPerPSO) :
	m_device(device), 
    m_contextsPerPSO(contextsPerPSO),
    m_psoList(psoList)
{
	for (int i = 0; i < m_psoList->GetCounts(); ++i)
	{
		ContextPool pool;
		pool.nextIndex = 0;
        auto pipelineEntry = m_psoList->Get(i);
		pool.pso = pipelineEntry.pso;
        pool.rs = pipelineEntry.rs;
		pool.contexts.resize(m_contextsPerPSO);

		for (Context& context : pool.contexts)
		{
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(context.allocator.ReleaseAndGetAddressOf())));
			NAME_D3D12_OBJECT(context.allocator);
			ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, context.allocator.Get(), nullptr, IID_PPV_ARGS(context.bundle.ReleaseAndGetAddressOf())));
			NAME_D3D12_OBJECT(context.bundle);
			context.bundle->Close();
		}
		m_pools.push_back(std::move(pool));
	}
}

// 핫 리로드된 PSOList에 맞춰서 Pool 재구성
bool BundleRecorder::SyncWithPSOList(const PSOList* psoList)
{
    if (!m_device)
    {
        return false;
    }

    const int newCount = psoList->GetCounts();
    m_pools.resize(newCount);

    for (int i = 0; i < newCount; ++i)
    {
        ContextPool& pool = m_pools[i];
        auto pipelineEntry = psoList->Get(i);

        bool needRebuildPool = false;

        if (pool.pso == nullptr || pool.rs == nullptr)
        {
            needRebuildPool = true;
        }
        else if (pool.pso.Get() != pipelineEntry.pso || pool.rs.Get() != pipelineEntry.rs)
        {
            needRebuildPool = true;
        }

        if (needRebuildPool)
        {
            pool.pso = pipelineEntry.pso;
            pool.rs = pipelineEntry.rs;
            pool.contexts.clear();
            pool.nextIndex = 0;
            pool.contexts.reserve(m_contextsPerPSO);

            for (size_t c = 0; c < m_contextsPerPSO; ++c)
            {
                Context ctx;
                ThrowIfFailed(m_device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(ctx.allocator.ReleaseAndGetAddressOf())));
                ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, ctx.allocator.Get(), pool.pso.Get(), IID_PPV_ARGS(ctx.bundle.ReleaseAndGetAddressOf())));
                ctx.bundle->Close();
                pool.contexts.push_back(std::move(ctx));
            }
        }
    }
    m_psoList = psoList;
	return true;
}

ID3D12GraphicsCommandList* BundleRecorder::RecordBundle(std::vector<ID3D12DescriptorHeap*>& heaps, const std::vector<DrawBindingInfo>& bindings, int psoIndex)
{
    auto pipeline = m_psoList->Get(psoIndex);
    ContextPool& pool = m_pools[size_t(psoIndex)];

    size_t& idx = pool.nextIndex;
    Context& context = pool.contexts[idx];
    idx = (idx + 1) % pool.contexts.size();

    context.allocator->Reset();

    ID3D12GraphicsCommandList* cmd = context.bundle.Get();
    cmd->Reset(context.allocator.Get(), pipeline.pso);
    cmd->SetGraphicsRootSignature(pipeline.rs);
    cmd->SetPipelineState(pipeline.pso);

    for (auto& bi : bindings)
    {
        RecordDrawItem(cmd, bi);
    }

    cmd->Close();
    return cmd;
}