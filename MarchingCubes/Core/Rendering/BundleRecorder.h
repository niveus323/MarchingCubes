#pragma once
#include <unordered_map>
using Microsoft::WRL::ComPtr;

interface IDrawable
{
public:
	virtual void Draw(ID3D12GraphicsCommandList* cmdList) const = 0;
	virtual void SetConstantsBuffers(ID3D12GraphicsCommandList* cmdList) const = 0;
	virtual ~IDrawable() = default;
};

struct StaticRenderItem
{
	ID3D12GraphicsCommandList* bundle;
};

struct DynamicRenderItem
{
	const IDrawable* object;
};

struct PendingDeleteItem
{
	UINT64 fenceValue; // 삭제 요청 시점의 fence 값
	std::vector<ComPtr<ID3D12Resource>> resources;
};

class BundleRecorder
{
public:
	BundleRecorder(ID3D12Device* device, ID3D12RootSignature* rootSignature, const std::unordered_map<PipelineMode, ComPtr<ID3D12PipelineState>>& psos, size_t contextsPerPSO = 2);

	StaticRenderItem CreateBundleFor(const std::vector<std::unique_ptr<IDrawable>>& drawables, PipelineMode mode);
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

	ID3D12Device* m_device;
	ID3D12RootSignature* m_rootSignature;
	std::vector<ContextPool> m_pools;
};

