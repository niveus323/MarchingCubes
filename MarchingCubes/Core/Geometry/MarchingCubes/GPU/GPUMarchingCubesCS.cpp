#include "pch.h"
#include "GPUMarchingCubesCS.h"
#include "Core/Geometry/MarchingCubesTables.h"

static const UINT64 tableSize = 256u * 16u * sizeof(UINT);

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
    const FrameAlloc& fa = context.fa;
    const RemeshRequest& req = context.req;
    
    XMUINT3 rmin = context.regionMin;
    XMUINT3 rmax = context.regionMax;
    XMUINT3 gridDim = vol.grid.cells;

    const UINT chunkCubes = vol.chunkCubes; // 16
    uint32_t groupsPerChunk = (chunkCubes + 7) / 8; // chunkcubes = 16이므로 2의 값이 들어갈 예정
    XMUINT3 gpc(groupsPerChunk, groupsPerChunk, groupsPerChunk);

    XMUINT3 cmin = { 
        rmin.x / chunkCubes, 
        rmin.y / chunkCubes, 
        rmin.z / chunkCubes 
    };
    XMUINT3 cmax = {
        (rmax.x + chunkCubes - 1) / chunkCubes,
        (rmax.y + chunkCubes - 1) / chunkCubes,
        (rmax.z + chunkCubes - 1) / chunkCubes
    };
    XMUINT3 expandedMin = { 
        cmin.x * chunkCubes, 
        cmin.y * chunkCubes, 
        cmin.z * chunkCubes 
    };
    XMUINT3 expandedMax = {
        std::min(cmax.x * chunkCubes, gridDim.x),
        std::min(cmax.y * chunkCubes, gridDim.y),
        std::min(cmax.z * chunkCubes, gridDim.z)
    };

    // t0 준비
    if (!m_tableUploaded)
    {
        // TriTable업로드를 위해 상태 전환
        D3D12_RESOURCE_BARRIER toCopy = CD3DX12_RESOURCE_BARRIER::Transition(m_triDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->ResourceBarrier(1, &toCopy);

        // 업로드
        cmd->CopyBufferRegion(m_triDefault.Get(), 0, m_triUpload.Get(), 0, tableSize);
        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_triDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &toSRV);

        // triTable SRV 작성 (t1)
        DescriptorHelper::CreateSRV_Structured(device, m_triDefault.Get(), sizeof(int), fa.descRing->StaticCpuAt(0));
        m_tableUploaded = true;
    }   

    // 디스패치(셸 개수=(dim-1))
    cmd->SetPipelineState(m_mcPso.Get());
    cmd->SetComputeRootSignature(m_mcRootSig.Get());

    // GridCB 업로드
    GridDesc grid = vol.grid;
    GridCBData cb{};
    cb.resolution = grid.cells;
    cb.scale = grid.cellsize; // 동일하다고 가정.
    cb.chunkOrigin = grid.origin;
    cb.isoValue = req.isoValue;
    cb.numChunkAxis = vol.numChunkAxis;
    cb.chunkCubes = chunkCubes;
    cb.regionMin = expandedMin;
    cb.regionMax = expandedMax;

    ConstantBufferHelper::SetRootCBV(device, cmd, 0, *fa.cbRing, fa.ringCursor, cb);
    ID3D12DescriptorHeap* heaps[] = { fa.descRing->GetHeap() };
    cmd->SetDescriptorHeaps(1, heaps);
    // SRV(t0)
    cmd->SetComputeRootDescriptorTable(1, fa.descRing->StaticGpuAt(0));
    // SRV(t1), UAV(u0)
    DescriptorHelper::SetTable(cmd, *fa.descRing, fa.ringCursor, { { 2, kSlot_t1 }, { 3, kSlot_u0 } });

#if PIX_DEBUGMODE
    PIXScopedEvent(PIX_COLOR(0, 128, 255), "CPU MC encode");
    PIXBeginEvent(cmd, PIX_COLOR(255, 64, 64), "MarchingCubesCS Dispatch");
#endif

    //cmd->Dispatch(context.numChunkAxis.x * gpc.x, context.numChunkAxis.y * gpc.y, context.numChunkAxis.z * gpc.z);

    const UINT gx = (cmax.x - cmin.x) * gpc.x;
    const UINT gy = (cmax.y - cmin.y) * gpc.y;
    const UINT gz = (cmax.z - cmin.z) * gpc.z;

    if (gx > 0 && gy > 0 && gz > 0)
    {
        cmd->Dispatch((cmax.x - cmin.x) * gpc.x, (cmax.y - cmin.y) * gpc.y, (cmax.z - cmin.z) * gpc.z);
    }
    
#if PIX_DEBUGMODE
    PIXEndEvent(cmd);
#endif
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

#ifdef _DEBUG
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif // _DEBUG

        // PSO (MarchingCubesCS.hlsl 의 "CS")
        ComPtr<ID3DBlob> csBlob, errorBlob;
        ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"MarchingCubesCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "MCMainCS", "cs_5_0", compileFlags, 0, &csBlob, nullptr));

        D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = m_mcRootSig.Get();
        pso.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
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