#include "pch.h"
#include "BundleRecorder.h"
#include "Core/Utils/DXHelper.h"

BundleRecorder::BundleRecorder(ID3D12Device* device, ID3D12PipelineState* pso, ID3D12RootSignature* rootSignature)
	: m_device(device), m_pso(pso), m_rootSignature(rootSignature)
{
}

ComPtr<ID3D12GraphicsCommandList> BundleRecorder::CreateBundleFor(const IDrawable* drawable, ID3D12CommandAllocator* allocator)
{
	// Create Allocator & CommandList
	ComPtr<ID3D12GraphicsCommandList> bundle;
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, allocator, m_pso.Get(), IID_PPV_ARGS(&bundle)));

	// Start Bundle Recording
	bundle->SetGraphicsRootSignature(m_rootSignature.Get());

	drawable->Draw(bundle.Get());

	// End Bundle Recording
	bundle->Close();

	return bundle;
}
