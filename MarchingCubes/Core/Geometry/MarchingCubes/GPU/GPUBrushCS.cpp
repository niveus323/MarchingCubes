#include "pch.h"
#include "GPUBrushCS.h"

GPUBrushCS::GPUBrushCS(ID3D12Device* device)
{
    ensureSignatures(device);
    ensurePipelines(device);
    //ensurePickBuffer(device);
    ensureRegionBuffer(device);
    ensureDispatchBuffer(device);
    ensureReadbacks(device);
}

void GPUBrushCS::encode(const GPUBrushEncodingContext& context)
{
    ID3D12Device* device = context.device;
    ID3D12GraphicsCommandList* cmd = context.cmd;
    const SDFVolumeView& vol = context.vol;
    const FrameAlloc& fa = context.fa;
    const BrushRequest& req = context.req;
    XMUINT3 brushCenter = { 
        static_cast<uint32_t>((req.hitpos.x - vol.grid.origin.x) / vol.grid.cellsize), 
        static_cast<uint32_t>((req.hitpos.y - vol.grid.origin.y) / vol.grid.cellsize), 
        static_cast<uint32_t>((req.hitpos.z - vol.grid.origin.z) / vol.grid.cellsize) 
    };

    ensureRegionBuffer(device);
    ensureDispatchBuffer(device);

    // u3, u4 작성 (u5, u6는 TerrainBackend에서 작성하였음)
    {
        DescriptorHelper::CreateUAV_Structured(device, m_regionBuffer.Get(), sizeof(UINT) * 4, fa.descRing->CpuAt(fa.ringCursor, kSlot_u3));
        DescriptorHelper::CreateUAV_Structured(device, m_dispatchArgsBuffer.Get(), sizeof(UINT) * 3, fa.descRing->CpuAt(fa.ringCursor, kSlot_u4));
    }

    GPUBrushRegionDispatchContext regionContext
    (
        device,
        cmd,
        vol,
        fa,
        req.radius,
        context.chunkCubes,
        brushCenter
    );

    DispatchBrushRegionCS(regionContext);

#ifdef _DEBUG
    //CopyToReadbacks(cmd);
#endif // _DEBUG

    // Density3D SRV -> UAV 전환
    {
        auto Density3DtoUav = CD3DX12_RESOURCE_BARRIER::Transition(vol.tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &Density3DtoUav);
    }

    // t2, u1 작성 (t2는 Pick 디스패치 직후에 작성되어 있음)
    {
        DescriptorHelper::CreateSRV_Structured(device, m_regionBuffer.Get(), sizeof(UINT) * 4, fa.descRing->CpuAt(fa.ringCursor, kSlot_t2));
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
    b.brushCenter = brushCenter;
    // b0 세팅
    ConstantBufferHelper::SetRootCBV(device, cmd, 0, *fa.cbRing, fa.ringCursor, b);
    // (t2, t3), u1 세팅
    DescriptorHelper::SetTable(cmd, *fa.descRing, fa.ringCursor, { {1, kSlot_t2} , {2, kSlot_u1} });

    cmd->SetPredication(m_dispatchArgsBuffer.Get(), 0, D3D12_PREDICATION_OP_EQUAL_ZERO);

#if PIX_DEBUGMODE
    PIXBeginEvent(cmd, PIX_COLOR(255, 0, 192), "BrushCS ExecuteIndirect");
    // Dispatch
    cmd->ExecuteIndirect(m_brushCmdSignature.Get(), 1, m_dispatchArgsBuffer.Get(), 0, nullptr, 0);
    PIXEndEvent(cmd);
#else
    // Dispatch
    cmd->ExecuteIndirect(m_brushCmdSignature.Get(), 1, m_dispatchArgsBuffer.Get(), 0, nullptr, 0);
#endif

    cmd->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);

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

void GPUBrushCS::FillFullGridRegionAndArgs(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const GridDesc& grid, std::vector<ComPtr<ID3D12Resource>>& pendingDeletes)
{
    XMUINT4 region[2] = {
        {0,0,0,0},
        { grid.cells.x, grid.cells.y, grid.cells.z, 0u }
    };

    XMUINT3 groups = {
        (grid.cells.x + 7u) / 8u,
        (grid.cells.y + 7u) / 8u,
        (grid.cells.z + 7u) / 8u
    };

    ComPtr<ID3D12Resource> uploadRegion, uploadArgs;
    {
        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(region));
        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadRegion)
        ));
        NAME_D3D12_OBJECT(uploadRegion);
        void* p = nullptr;
        uploadRegion->Map(0, nullptr, &p);
        memcpy(p, region, sizeof(region));
        uploadRegion->Unmap(0, nullptr);
    }

    {
        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT) * 3);
        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadArgs)
        ));
        NAME_D3D12_OBJECT(uploadArgs);

        void* p = nullptr;
        uploadArgs->Map(0, nullptr, &p);
        memcpy(p, &groups, sizeof(XMUINT3));
        uploadArgs->Unmap(0, nullptr);
    }

    D3D12_RESOURCE_BARRIER toCopy[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_regionBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
        CD3DX12_RESOURCE_BARRIER::Transition(m_dispatchArgsBuffer.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)
    };
    cmd->ResourceBarrier(2, toCopy);
    cmd->CopyBufferRegion(m_regionBuffer.Get(), 0, uploadRegion.Get(), 0, sizeof(region));
    cmd->CopyBufferRegion(m_dispatchArgsBuffer.Get(), 0, uploadArgs.Get(), 0, sizeof(XMUINT3));

    D3D12_RESOURCE_BARRIER afterCopy[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_regionBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_dispatchArgsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
    };
    cmd->ResourceBarrier(2, afterCopy);

    pendingDeletes.push_back(uploadRegion);
    pendingDeletes.push_back(uploadArgs);
}

void GPUBrushCS::ensureSignatures(ID3D12Device* device)
{
    // BurushRegionCS 시그니쳐 : b0, u3, u4, u5, u6
    if (!m_brushRegionRootSignature)
    {
        CD3DX12_DESCRIPTOR_RANGE1 sUav{};
        sUav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // u3, u4, u5, u6

        CD3DX12_ROOT_PARAMETER1 param[2]{};
        param[0].InitAsConstantBufferView(0); // b0 : RegionArgsCB
        param[1].InitAsDescriptorTable(1, &sUav); // u3,: OutRegion, u4 : OutDispatchArgs , u5 : OutChunkMasks, u6 : OutChunkCounter

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Init_1_1(_countof(param), param, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);

        ComPtr<ID3DBlob> blob, err;
        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rsDesc, &blob, &err));
        ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&m_brushRegionRootSignature)));
    }

    // Brush 시그니쳐 : b0, t2, u1
    if (!m_brushRootSignature)
    {
        CD3DX12_DESCRIPTOR_RANGE1 sSrv{}, uUav{};
        sSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // t2
        uUav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // u1

        CD3DX12_ROOT_PARAMETER1 params[3]{};
        params[0].InitAsConstantBufferView(0); // b0 : BrushCB
        params[1].InitAsDescriptorTable(1, &sSrv); // t2 : RegionBuffer 
        params[2].InitAsDescriptorTable(1, &uUav); // u1 : editTexture

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

    if (!m_brushCmdSignature)
    {
        // BrushCS에 ExecuteIndriect 하기 위한 CommandSignarue
        D3D12_INDIRECT_ARGUMENT_DESC arg{};
        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH; // Dispatch 목적

        D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
        sigDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        sigDesc.NumArgumentDescs = 1;
        sigDesc.pArgumentDescs = &arg;

        // BrushRegionCS로 도출한 하나의 영역만 Indirect 디스패치를 하기 때문에 여러 영역을 한 프레임에 처리할 일 없음
        // -> 루트 오퍼랜드가 필요없다
        ThrowIfFailed(device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&m_brushCmdSignature)));
    }
}

void GPUBrushCS::ensurePipelines(ID3D12Device* device)
{
#ifdef _DEBUG
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif // _DEBUG

    if (!m_brushRegionPso)
    {
        ComPtr<ID3DBlob> brushRegionCS;
        ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"BrushRegionCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "cs_5_0", compileFlags, 0, &brushRegionCS, nullptr));

        D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
        d.pRootSignature = m_brushRegionRootSignature.Get();
        d.CS = { brushRegionCS->GetBufferPointer(), brushRegionCS->GetBufferSize() };
        ThrowIfFailed(device->CreateComputePipelineState(&d, IID_PPV_ARGS(&m_brushRegionPso)));
    }


    if (!m_brushPso)
    {
        ComPtr<ID3DBlob> brushCS;
        ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"BrushCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BrushCS", "cs_5_0", compileFlags, 0, &brushCS, nullptr));

        D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
        d.pRootSignature = m_brushRootSignature.Get();
        d.CS = { brushCS->GetBufferPointer(), brushCS->GetBufferSize() };
        ThrowIfFailed(device->CreateComputePipelineState(&d, IID_PPV_ARGS(&m_brushPso)));
    }

}

void GPUBrushCS::ensureRegionBuffer(ID3D12Device* device)
{
    if (!m_regionBuffer)
    {
        UINT stride = sizeof(uint32_t) * 4; // padding 포함하여 uint4(16-bytes) 단위
        UINT numElements = 2; // uint4 2개 [0],[1]
        UINT bytes = stride * numElements;
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_regionBuffer)));
        NAME_D3D12_OBJECT(m_regionBuffer);
    }

}

void GPUBrushCS::ensureDispatchBuffer(ID3D12Device* device)
{
    if (m_dispatchArgsBuffer) return;

    UINT stride = sizeof(UINT) * 3;
    UINT numELements = 1;
    UINT bytes = stride * numELements;
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_dispatchArgsBuffer)));
    NAME_D3D12_OBJECT(m_dispatchArgsBuffer);
}

void GPUBrushCS::ensureReadbacks(ID3D12Device* device)
{
    // 필요 시 사용할 것.

   /* 
    if (!m_dispatchArgsReadback)
    {
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT32) * 3);
        auto hpReadback = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        ThrowIfFailed(device->CreateCommittedResource(&hpReadback, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_dispatchArgsReadback)));
    }

    if (!m_regionReadback)
    {
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(OutRegion));
        auto hpReadback = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        ThrowIfFailed(device->CreateCommittedResource(&hpReadback, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_regionReadback)));
    }*/

}

void GPUBrushCS::DispatchBrushRegionCS(const GPUBrushRegionDispatchContext& context)
{
#if PIX_DEBUGMODE
    PIXScopedEvent(PIX_COLOR(0, 128, 255), "CPU BrushRegionCS encode");
#endif
    ID3D12Device* device = context.device;
    ID3D12GraphicsCommandList* cmd = context.cmd;
    const SDFVolumeView& vol = context.vol;
    const FrameAlloc& fa = context.fa;

    // OutRegion, OutDispatchArgs 초기 Transition
    {
        CD3DX12_RESOURCE_BARRIER toUavs[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_regionBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_dispatchArgsBuffer.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        cmd->ResourceBarrier(_countof(toUavs), toUavs);
    }

    // PSO, RootSignature, Heap 세팅
    cmd->SetPipelineState(m_brushRegionPso.Get());
    cmd->SetComputeRootSignature(m_brushRegionRootSignature.Get());
    RegionArgsCBData cb{};
    cb.cells = vol.grid.cells;
    cb.cellSize = vol.grid.cellsize;
    cb.radius = context.radius;
    cb.halo = 1;
    cb.chunkCubes = context.chunkCubes;
    cb.brushCenter = context.brushCeneter;
    // b0, (u3, u4) GPU 세팅
    ConstantBufferHelper::SetRootCBV(device, cmd, 0, *fa.cbRing, fa.ringCursor, cb);
    DescriptorHelper::SetTable(cmd, *fa.descRing, fa.ringCursor, { { 1, kSlot_u3 } });

#if PIX_DEBUGMODE
    PIXBeginEvent(cmd, PIX_COLOR(0, 192, 255), "Brush Region CS Dispatch");
    cmd->Dispatch(1, 1, 1);
    PIXEndEvent(cmd);
#else
    cmd->Dispatch(1, 1, 1);
#endif

    // BrushCS에서 사용할 수 있도록 Transition
    D3D12_RESOURCE_BARRIER barrierAfterRegionCS[2] = {
           CD3DX12_RESOURCE_BARRIER::Transition(m_regionBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE), // u3 -> t3
           CD3DX12_RESOURCE_BARRIER::Transition(m_dispatchArgsBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)    // u4 -> IndirectArgs
    };
    cmd->ResourceBarrier(_countof(barrierAfterRegionCS), barrierAfterRegionCS);
}
