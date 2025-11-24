#include "pch.h"
#include "GPUBrushCS.h"
#include "Core/Rendering/PSO/PSOList.h"

GPUBrushCS::GPUBrushCS(ID3D12Device* device)
{
	ensureRootSignature(device);
	ensurePipeline(device);
}

void GPUBrushCS::encode(const GPUBrushEncodingContext& context)
{
	ID3D12GraphicsCommandList* cmd = context.cmd;
	const SDFVolumeView& vol = context.vol;

	// 브러시용 threadgroups
	const XMUINT3 brushGroups = computeBrushDispatchGroups(context. regionMin, context.regionMax);

	// Density3D SRV -> UAV 전환
	auto Density3DtoUav = CD3DX12_RESOURCE_BARRIER::Transition(vol.tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmd->ResourceBarrier(1, &Density3DtoUav);

#if PIX_DEBUGMODE
	PIXScopedEvent(PIX_COLOR(0, 128, 255), "CPU Brush encode");
#endif

	// PSO, RootSig, Heap 세팅
	cmd->SetPipelineState(m_brushPso.Get());
	cmd->SetComputeRootSignature(m_brushRootSignature.Get());
	cmd->SetComputeRootConstantBufferView(0, context.cbAddress);
	cmd->SetComputeRootDescriptorTable(1, vol.uav);

#if PIX_DEBUGMODE
	PIXBeginEvent(cmd, PIX_COLOR(255, 0, 192), "BrushCS ExecuteIndirect");
#endif

	// Dispatch
	cmd->Dispatch(brushGroups.x, brushGroups.y, brushGroups.z);

#if PIX_DEBUGMODE
	PIXEndEvent(cmd);
#endif

	// Dispatch 후 상태 전환
	auto backToSrv = CD3DX12_RESOURCE_BARRIER::Transition(vol.tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cmd->ResourceBarrier(1, &backToSrv);
}

void GPUBrushCS::ensureRootSignature(ID3D12Device* device)
{
	// Brush 시그니쳐 : b0, u1
	if (!m_brushRootSignature)
	{
		CD3DX12_DESCRIPTOR_RANGE1 uUav{};
		uUav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // u1

		CD3DX12_ROOT_PARAMETER1 params[2]{};
		params[0].InitAsConstantBufferView(0); // b0 : BrushCB
		params[1].InitAsDescriptorTable(1, &uUav); // u1 : editTexture

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
		rsDesc.Init_1_1(_countof(params), params, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);

		ComPtr<ID3DBlob> blob, err;
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rsDesc, &blob, &err));
		ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_brushRootSignature)));
	}
}

void GPUBrushCS::ensurePipeline(ID3D12Device* device)
{
	if (!m_brushPso)
	{
		ComPtr<ID3DBlob> brushCS = PSOList::LoadFileBlob("BrushCS.cso");;
		
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{
			.pRootSignature = m_brushRootSignature.Get(),
			.CS = { brushCS->GetBufferPointer(), brushCS->GetBufferSize() }
		};
		ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_brushPso.ReleaseAndGetAddressOf())));
	}
}

DirectX::XMUINT3 GPUBrushCS::computeBrushDispatchGroups(const DirectX::XMUINT3& regionMin, const DirectX::XMUINT3& regionMax)
{
	XMUINT3 extent{
		regionMax.x - regionMin.x,
		regionMax.y - regionMin.y,
		regionMax.z - regionMin.z
	};

	XMUINT3 cubeExtent{
		extent.x > 0 ? (extent.x - 1) : 0,
		extent.y > 0 ? (extent.y - 1) : 0,
		extent.z > 0 ? (extent.z - 1) : 0
	};
	
	return {
		std::max(1u, (cubeExtent.x + 7) / 8),
		std::max(1u, (cubeExtent.y + 7) / 8),
		std::max(1u, (cubeExtent.z + 7) / 8)
	};
}
