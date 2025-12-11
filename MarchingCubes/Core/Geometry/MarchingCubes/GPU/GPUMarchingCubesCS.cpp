#include "pch.h"
#include "GPUMarchingCubesCS.h"
#include "Core/Geometry/MarchingCubes/MarchingCubesTables.h"
#include "Core/Rendering/PSO/PSOList.h"

static const uint64_t tableSize = 256u * 16u * sizeof(uint32_t);

GPUMarchingCubesCS::GPUMarchingCubesCS(ID3D12Device* device)
{
	ensurePipeline(device);
	ensureTable(device);
}

void GPUMarchingCubesCS::encode(const GPUMCEncodingContext& context)
{
	ID3D12Device* device = context.device;
	ID3D12GraphicsCommandList* cmd = context.cmd;
	const SDFVolumeView& vol = context.vol;
	const RemeshRequest& req = context.req;
	XMUINT3 dispatchGroups = computeDispatchGroupsForRegion(vol.chunkCubes, vol.grid.cells, context.regionCellMin, context.regionCellMax);

	uploadTableIfNeeded(cmd);

	// 디스패치(셸 개수=(dim-1))
	cmd->SetPipelineState(m_mcPso.Get());
	cmd->SetComputeRootSignature(m_mcRootSig.Get());
	cmd->SetComputeRootConstantBufferView(0, context.cbAddress); // b0: GridCB
	cmd->SetComputeRootDescriptorTable(1, context.triTableSrv); // t0: triTable
	cmd->SetComputeRootDescriptorTable(2, context.vol.srv); // t1: Density3D
	cmd->SetComputeRootDescriptorTable(3, context.outBufferUav); // u0: OutTriangle UAV

#if PIX_DEBUGMODE
	PIXScopedEvent(PIX_COLOR(0, 128, 255), "CPU MC encode");
	PIXBeginEvent(cmd, PIX_COLOR(255, 64, 64), "MarchingCubesCS Dispatch");
#endif
	if (dispatchGroups.x > 0 && dispatchGroups.y > 0 && dispatchGroups.z > 0)
	{
		cmd->Dispatch(dispatchGroups.x, dispatchGroups.y, dispatchGroups.z);
	}
#if PIX_DEBUGMODE
	PIXEndEvent(cmd);
#endif
}

void GPUMarchingCubesCS::uploadTableIfNeeded(ID3D12GraphicsCommandList* cmd)
{
	if (m_tableUploaded) return;

	D3D12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(m_triDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	cmd->ResourceBarrier(1, &toCopy);

	cmd->CopyBufferRegion(m_triDefault.Get(), 0, m_triUpload.Get(), 0, tableSize);

	auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(m_triDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cmd->ResourceBarrier(1, &toSrv);

	m_tableUploaded = true;
}

void GPUMarchingCubesCS::ensurePipeline(ID3D12Device* device)
{
	// MarchingCubesCS 시그니쳐 : b0, t0, t1, u0
	if (!m_mcRootSig)
	{
		CD3DX12_DESCRIPTOR_RANGE1 sSRV[2]{}, sUav{};
		sSRV[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 (static)
		sSRV[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // t1
		sUav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // u0

		CD3DX12_ROOT_PARAMETER1 params[4]{};
		params[0].InitAsConstantBufferView(0);          // b0 : GridCB
		params[1].InitAsDescriptorTable(1, &sSRV[0]);   // t0 : triTable
		params[2].InitAsDescriptorTable(1, &sSRV[1]);   // t1 : Density3D
		params[3].InitAsDescriptorTable(1, &sUav);      // u0 : OutBuffer

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};

		rsDesc.Init_1_1(_countof(params), params, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);

		ComPtr<ID3DBlob> rsBlob, err;
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rsDesc, &rsBlob, &err));
		ThrowIfFailed(device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(m_mcRootSig.ReleaseAndGetAddressOf())));
	}

	if (!m_mcPso)
	{
		// PSO (MarchingCubesCS.hlsl 의 "CS")
		ComPtr<ID3DBlob> csBlob = PSOList::LoadFileBlob("MarchingCubesCS.cso");

		D3D12_COMPUTE_PIPELINE_STATE_DESC pso{
			.pRootSignature = m_mcRootSig.Get(),
			.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() }
		};
		ThrowIfFailed(device->CreateComputePipelineState(&pso, IID_PPV_ARGS(m_mcPso.ReleaseAndGetAddressOf())));
	}
}

// 정적 데이터인 Tritable은 초기화 단계에서 미리 생성해둔다
void GPUMarchingCubesCS::ensureTable(ID3D12Device* device)
{
	if (m_triDefault) return;

	CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(tableSize);

	device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_triDefault.ReleaseAndGetAddressOf()));
	NAME_D3D12_OBJECT(m_triDefault);
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_triUpload.ReleaseAndGetAddressOf()));
	NAME_D3D12_OBJECT(m_triUpload);

	void* p = nullptr;
	ThrowIfFailed(m_triUpload->Map(0, nullptr, &p));
	std::memcpy(p, MarchingCubesTables::triTable, tableSize);
	m_triUpload->Unmap(0, nullptr);
}

DirectX::XMUINT3 GPUMarchingCubesCS::computeDispatchGroupsForRegion(const uint32_t chunkcubes, const DirectX::XMUINT3& gridDim, const DirectX::XMUINT3& regionMin, const DirectX::XMUINT3& regionMax)
{
	uint32_t groupsPerChunk = (chunkcubes + 7) / 8; // chunkcubes = 16이므로 2의 값이 들어갈 예정
	XMUINT3 gpc(groupsPerChunk, groupsPerChunk, groupsPerChunk);

	XMUINT3 cmin{
		regionMin.x / chunkcubes,
		regionMin.y / chunkcubes,
		regionMin.z / chunkcubes
	};

	XMUINT3 cmax{
		(regionMax.x + chunkcubes - 1) / chunkcubes,
		(regionMax.y + chunkcubes - 1) / chunkcubes,
		(regionMax.z + chunkcubes - 1) / chunkcubes
	};

	XMUINT3 expandedMin{
		cmin.x * chunkcubes,
		cmin.y * chunkcubes,
		cmin.z * chunkcubes
	};

	XMUINT3 expandedMax{
		std::min(cmax.x * chunkcubes, gridDim.x),
		std::min(cmax.y * chunkcubes, gridDim.y),
		std::min(cmax.z * chunkcubes, gridDim.z)
	};

	return {
		(cmax.x - cmin.x) * gpc.x,
		(cmax.y - cmin.y) * gpc.y,
		(cmax.z - cmin.z) * gpc.z
	};
}
