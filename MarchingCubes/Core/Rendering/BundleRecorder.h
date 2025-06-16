#pragma once
using Microsoft::WRL::ComPtr;

interface IDrawable
{
public:
	virtual void Draw(ID3D12GraphicsCommandList* cmd) const = 0;
	virtual UINT GetIndexCount() const = 0;
	virtual ~IDrawable() = default;
};

class BundleRecorder
{
public:
	BundleRecorder(ID3D12Device* device, ID3D12PipelineState* pso, ID3D12RootSignature* rootSignature);

	ComPtr<ID3D12GraphicsCommandList> CreateBundleFor(const IDrawable* drawable, ID3D12CommandAllocator* allocator);
private:
	ID3D12Device* m_device;
	ComPtr<ID3D12PipelineState> m_pso;
	ComPtr<ID3D12RootSignature> m_rootSignature;
};

