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
#if PIX_DEBUGMODE
    PIXScopedEvent(PIX_COLOR(0, 128, 255), "CPU MC encode");
#endif

    ID3D12Device* device = context.device;
    ID3D12GraphicsCommandList* cmd = context.cmd;
    const SDFVolumeView& vol = context.vol;
    const FrameAlloc& fa = context.fa;
    const RemeshRequest& req = context.req;

    // t0 준비
    if (!m_tableUploaded)
    {
        // TriTable업로드를 위해 상태 전환
        {
            D3D12_RESOURCE_BARRIER toCopy[2] = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_triDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
                CD3DX12_RESOURCE_BARRIER::Transition(m_triUpload.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ)
            };
            cmd->ResourceBarrier(2, toCopy);
        }

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
    cb.numChunkAxis = context.numChunkAxis;
    cb.chunkCubes = context.chunkCubes;
    uint32_t groupsPerChunk = (context.chunkCubes + 7) / 8;
    XMUINT3 gpc(groupsPerChunk, groupsPerChunk, groupsPerChunk);
    cb.groupsPerChunk = gpc;
    cb.chunkCapacity = context.chunkCapacity;
    
    ConstantBufferHelper::SetRootCBV(device, cmd, 0, *fa.cbRing, fa.ringCursor, cb);
    // SRV(t1, t3), UAV(u0, u6)
    DescriptorHelper::SetTable(cmd, *fa.descRing, fa.ringCursor, { { 2, kSlot_t1 }, {3, kSlot_t3}, { 4, kSlot_u0 } , {5, kSlot_u6} });
    // SRV(t0)
    cmd->SetComputeRootDescriptorTable(1, fa.descRing->StaticGpuAt(0));

#if PIX_DEBUGMODE
    PIXBeginEvent(cmd, PIX_COLOR(255, 64, 64), "MarchingCubesCS Dispatch");
    cmd->Dispatch(context.numChunkAxis.x * gpc.x, context.numChunkAxis.y * gpc.y, context.numChunkAxis.z * gpc.z);
    PIXEndEvent(cmd);
#else
    cmd->Dispatch(context.numChunkAxis.x * gpc.x, context.numChunkAxis.y * gpc.y, context.numChunkAxis.z * gpc.z);
#endif
}

void GPUMarchingCubesCS::ensurePipeline(ID3D12Device* device)
{
    // MarchingCubesCS 시그니쳐 : b0, t0, t1, t4, u0, u6
    if (!m_mcRootSig)
    {
        CD3DX12_DESCRIPTOR_RANGE1 sSRV[3]{}, sUav[2]{};
        sSRV[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 (static)
        sSRV[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // t1
        sSRV[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // t3
        sUav[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // u0
        sUav[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // u6

        CD3DX12_ROOT_PARAMETER1 params[6]{};
        params[0].InitAsConstantBufferView(0);          // b0 : GridCB
        for (size_t i = 0; i < _countof(sSRV); ++i)
        {
            params[i + 1].InitAsDescriptorTable(1, &sSRV[i]);
        }
        for (size_t i = 0; i < _countof(sUav); ++i)
        {
            params[i + 1 + _countof(sSRV)].InitAsDescriptorTable(1, &sUav[i]);
        }
        
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};

        rsDesc.Init_1_1(_countof(params), params, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);

        ComPtr<ID3DBlob> rsBlob, err;
        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rsDesc, &rsBlob, &err));
        ThrowIfFailed(device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&m_mcRootSig)));
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
        ThrowIfFailed(device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_mcPso)));
    }

    if (!m_mcCmdSig)
    {
        D3D12_INDIRECT_ARGUMENT_DESC arg{};
        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC sig{};
        sig.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        sig.NumArgumentDescs = 1;
        sig.pArgumentDescs = &arg;

        ThrowIfFailed(device->CreateCommandSignature(&sig, nullptr, IID_PPV_ARGS(&m_mcCmdSig)));
    }
}

// 정적 데이터인 Tritable은 초기화 단계에서 미리 생성해둔다
void GPUMarchingCubesCS::ensureTable(ID3D12Device* device)
{
    if (m_triDefault) return;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(tableSize);

    device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_triDefault));
    NAME_D3D12_OBJECT(m_triDefault);
    device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_triUpload));
    NAME_D3D12_OBJECT(m_triUpload);

    void* p = nullptr;
    ThrowIfFailed(m_triUpload->Map(0, nullptr, &p));
    std::memcpy(p, MarchingCubesTables::triTable, tableSize);
    m_triUpload->Unmap(0, nullptr);
}