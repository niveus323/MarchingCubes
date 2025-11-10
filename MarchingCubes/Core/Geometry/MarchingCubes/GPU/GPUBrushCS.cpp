#include "pch.h"
#include "GPUBrushCS.h"

GPUBrushCS::GPUBrushCS(ID3D12Device* device)
{
    ensureSignatures(device);
    ensurePipelines(device);
}

void GPUBrushCS::encode(const GPUBrushEncodingContext& context)
{
    ID3D12Device* device = context.device;
    ID3D12GraphicsCommandList* cmd = context.cmd;
    const SDFVolumeView& vol = context.vol;
    const FrameAlloc& fa = context.fa;
    const BrushRequest& req = context.req;
    const XMUINT3 brushcenter = context.brushCenter;
    const XMUINT3 regionMin = context.regionMin;
    const XMUINT3 regionMax = context.regionMax;

    // 브러시용 threadgroups
    auto ext = XMUINT3{ 
        regionMax.x - regionMin.x, 
        regionMax.y - regionMin.y, 
        regionMax.z - regionMin.z 
    };
    auto cubeExt = XMUINT3{
        ext.x ? (ext.x - 1) : 0,
        ext.y ? (ext.y - 1) : 0,
        ext.z ? (ext.z - 1) : 0
    };
    XMUINT3 brushGroups = {
        std::max(1u, (cubeExt.x + 7) / 8),
        std::max(1u, (cubeExt.y + 7) / 8),
        std::max(1u, (cubeExt.z + 7) / 8)
    };

    // Density3D SRV -> UAV 전환
    {
        auto Density3DtoUav = CD3DX12_RESOURCE_BARRIER::Transition(vol.tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &Density3DtoUav);
    }

    // u1 작성
    {
        DescriptorHelper::CreateUAV_Texture3D(device, vol.tex, DXGI_FORMAT_R32_FLOAT, fa.descRing->CpuAt(fa.ringCursor, kSlot_u1));
    }

#if PIX_DEBUGMODE
    PIXScopedEvent(PIX_COLOR(0, 128, 255), "CPU Brush encode");
#endif

    // PSO, RootSig, Heap 세팅
    cmd->SetPipelineState(m_brushPso.Get());
    cmd->SetComputeRootSignature(m_brushRootSignature.Get());

    GridDesc gridDesc = vol.grid;

    BrushCBData b{};
    b.resolution = gridDesc.cells.x; // 동일하다고 가정 (100x100x100)
    b.radius = req.radius;
    b.deltaTime = req.deltaTime;
    b.weight = req.weight;
    b.cells = gridDesc.cells;
    b.brushCenter = brushcenter;
    b.regionMin= regionMin;
    b.regionMax= regionMax;
    // b0 세팅
    ConstantBufferHelper::SetRootCBV(device, cmd, 0, *fa.cbRing, fa.ringCursor, b);
    ID3D12DescriptorHeap* heaps[] = { fa.descRing->GetHeap() };
    cmd->SetDescriptorHeaps(1, heaps);
    // u1 세팅
    DescriptorHelper::SetTable(cmd, *fa.descRing, fa.ringCursor, { {1, kSlot_u1} });

#if PIX_DEBUGMODE
    PIXBeginEvent(cmd, PIX_COLOR(255, 0, 192), "BrushCS ExecuteIndirect");
    // Dispatch
    cmd->Dispatch(brushGroups.x, brushGroups.y, brushGroups.z);
    PIXEndEvent(cmd);
#else
    // Dispatch
    cmd->Dispatch(brushGroups.x, brushGroups.y, brushGroups.z);
#endif

    // Dispatch 후 상태 전환
    {
        D3D12_RESOURCE_BARRIER barriers[1] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            vol.tex,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        };
        cmd->ResourceBarrier(_countof(barriers), barriers);

    }    
}

void GPUBrushCS::ensureSignatures(ID3D12Device* device)
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
        ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&m_brushRootSignature)));
    }
}

void GPUBrushCS::ensurePipelines(ID3D12Device* device)
{
#ifdef _DEBUG
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif // _DEBUG

    if (!m_brushPso)
    {
        ComPtr<ID3DBlob> brushCS;
        ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"BrushCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BrushCS", "cs_5_0", compileFlags, 0, &brushCS, nullptr));

        D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
        d.pRootSignature = m_brushRootSignature.Get();
        d.CS = { brushCS->GetBufferPointer(), brushCS->GetBufferSize() };
        ThrowIfFailed(device->CreateComputePipelineState(&d, IID_PPV_ARGS(m_brushPso.ReleaseAndGetAddressOf())));
    }

}
