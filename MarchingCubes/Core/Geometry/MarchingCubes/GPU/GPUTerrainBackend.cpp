#include "pch.h"
#include "GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/TerrainChunkRenderer.h"
#include "Core/Geometry/MarchingCubes/GPU/SDFVolume3D.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUBrushCS.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesCS.h"

static inline ChunkKey DecodeChunkKey(UINT idx, const XMUINT3& cells)
{
    const UINT nx = cells.x, ny = cells.y, nxy = nx * ny;
    ChunkKey k;
    k.z = idx / nxy;
    idx -= k.z * nxy;
    k.y = idx / nx;
    k.x = idx - k.y * nx;
    return k;
}

GPUTerrainBackend::GPUTerrainBackend(ID3D12Device* device, const GridDesc& gridDesc) :
    m_device(device),
    m_grid(gridDesc),
    m_fenceEvent(nullptr)
{
    m_vol = std::make_unique<SDFVolume3D>(device);
    m_brush = std::make_unique<GPUBrushCS>(device);
    m_mc = std::make_unique<GPUMarchingCubesCS>(device);

    UINT totalBytesPerFrame = ConstantBufferHelper::CalcBytesPerFrame({
        { sizeof(BrushCBData), 1 },
        { sizeof(GridCBData), 1 }
        });

    m_cbRing = std::make_unique<ConstantBufferHelper::CBRing>(m_device, m_ring, totalBytesPerFrame);
    m_descriptorRing = std::make_unique<DescriptorHelper::DescriptorRing>(m_device, m_ring, kSlot_CountPerFrame, 1);


    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    NAME_D3D12_OBJECT(m_commandQueue);

    for (UINT n = 0; n < kRBFrameCount; ++n)
    {
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_commandAllocator[n])));
        NAME_D3D12_OBJECT_INDEXED(m_commandAllocator, n);
    }

    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    NAME_D3D12_OBJECT(m_fence);
    m_fenceValues = 1;
    m_lastSubmitFenceValues = 0;
    
    //m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_commandAllocator[m_rbCursor].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_commandList->Close());
}

GPUTerrainBackend::~GPUTerrainBackend()
{
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
    }
}

void GPUTerrainBackend::setGridDesc(const GridDesc& desc)
{
    m_grid = desc;

    computeNumChunks();
    // readback 버퍼 재생성
    m_outBuffer.Reset();
    ensureTriangleBuffer();
    for (UINT i = 0; i < kRBFrameCount; ++i)
    {
        resetRBSlot(i);
        ensureRBSlot(i);
    }
}

void GPUTerrainBackend::setFieldPtr(std::shared_ptr<SdfField<float>> grid)
{
    m_gridData = std::move(grid);
    //SDFVolume3D의 Upload는 m_fieldDirty를 체크하여 encode에서 처리.
    m_fieldDirty = true;
}

void GPUTerrainBackend::requestBrush(const BrushRequest& r)
{
    m_requestedBrush = r;
    m_hasBrush = true;

    // Brush를 처리한다는건 메쉬 갱신이 필요하다는 뜻
    requestRemesh(RemeshRequest(m_requestedRemesh.isoValue));
}

// 전체 Chunk 리빌드 요청
void GPUTerrainBackend::requestRemesh(const RemeshRequest& r)
{
    m_requestedRemesh = r;
    m_needsRemesh = true;

    encode();
}

void GPUTerrainBackend::encode()
{
    // encode가 발동하는 조건 : _GRD 갱신(m_fieldDirty == true), brush 사용(m_hasBrush == true)
    if (!m_device || (!m_fieldDirty && !m_hasBrush && !m_needsRemesh)) return;

    ThrowIfFailed(m_commandAllocator[m_rbCursor]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator[m_rbCursor].Get(), nullptr));

    m_cbRing->BeginFrame(m_ringCursor);

    FrameAlloc fa{};
    fa.cbRing = m_cbRing.get();
    fa.descRing = m_descriptorRing.get();
    fa.ringCursor = m_ringCursor;

    if (m_fieldDirty)
    {
        m_vol->uploadFromGRD(m_commandList.Get(), m_gridData.get(), m_pendingDeletes);

        m_fieldDirty = false;
        m_needsRemesh = true;
    }

    SDFVolumeView volView{};
    volView.grid = m_grid;
    volView.tex = m_vol->density();
    volView.srv = m_descriptorRing->GpuAt(m_ringCursor, kSlot_t1);
    volView.uav = m_descriptorRing->GpuAt(m_ringCursor, kSlot_u1);
    volView.chunkCubes = s_chunkcubes;
    volView.numChunkAxis = m_numChunkAxis;

#if PIX_DEBUGMODE
    if (m_hasBrush && PIXGetCaptureState() > 0)
    {
        PIXBeginEvent(m_commandList.Get(), PIX_COLOR(0, 255, 0), "GPUTerrainBackend");
    }
#endif

    XMUINT3 regionMin = { 0,0,0 };
    XMUINT3 regionMax = m_grid.cells;

    if (m_hasBrush)
    {
        const int halo = 1;
        const int r = (int)std::ceil(m_requestedBrush.radius / m_grid.cellsize);
        
        XMUINT3 brushCenter = {
            (UINT)((m_requestedBrush.hitpos.x - m_grid.origin.x) / m_grid.cellsize),
            (UINT)((m_requestedBrush.hitpos.y - m_grid.origin.y) / m_grid.cellsize),
            (UINT)((m_requestedBrush.hitpos.z - m_grid.origin.z) / m_grid.cellsize)
        };

        XMUINT3 gridDim = m_grid.cells;

        regionMin = {
            (UINT)std::max(0,  (int)brushCenter.x - r + halo),
            (UINT)std::max(0,  (int)brushCenter.y - r + halo),
            (UINT)std::max(0,  (int)brushCenter.z - r + halo)
        };
        regionMax = {
            (UINT)std::min((int)gridDim.x, (int)brushCenter.x + r + halo),
            (UINT)std::min((int)gridDim.y, (int)brushCenter.y + r + halo),
            (UINT)std::min((int)gridDim.z, (int)brushCenter.z + r + halo)
        };

        GPUBrushEncodingContext context(
            m_device, 
            m_commandList.Get(),
            volView, 
            fa, 
            m_requestedBrush,
            brushCenter,
            regionMin,
            regionMax
        );
        m_brush->encode(context);
        m_hasBrush = false;
    }

    if (m_needsRemesh)
    {
        // t0, u0(+counter) 준비
        {
            DescriptorHelper::CreateSRV_Texture3D(m_device, m_vol->density(), DXGI_FORMAT_R32_FLOAT, fa.descRing->CpuAt(fa.ringCursor, kSlot_t1));
            DescriptorHelper::CreateUAV_Structured(m_device, m_outBuffer.Get(), sizeof(OutTriangle), m_descriptorRing->CpuAt(m_ringCursor, kSlot_u0), m_outCounter.Get());
        }

        auto trisToUav = CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_commandList->ResourceBarrier(1, &trisToUav);

        // 카운터 리셋(0)
        MCUtil::ResetAndTransitCounter(m_device, m_commandList.Get(), m_outCounter.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        GPUMCEncodingContext context(
            m_device, 
            m_commandList.Get(),
            volView, 
            fa, 
            m_requestedRemesh,
            regionMin,
            regionMax
        );

        m_mc->encode(context);

        // readback 스케쥴
        const UINT rbSlot = m_rbCursor;
        ensureRBSlot(rbSlot);
        
        // Output Readback
        {
            CD3DX12_RESOURCE_BARRIER toCopySrc[2] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(m_outCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
            };
            m_commandList->ResourceBarrier(_countof(toCopySrc), toCopySrc);

            m_commandList->CopyBufferRegion(m_rb[rbSlot].rbCount.Get(), 0, m_outCounter.Get(), 0, sizeof(UINT));
            
            CD3DX12_RESOURCE_BARRIER backToUav[2] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(m_outCounter.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            };
            m_commandList->ResourceBarrier(_countof(backToUav), backToUav);
        }
        m_needsRemesh = false;
    }

#if PIX_DEBUGMODE
    if (PIXGetCaptureState() > 0 )
    {
        PIXEndEvent(m_commandList.Get());
    }
#endif
    //Wait For GPU
    {
        m_commandList->Close();
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Compute 큐의 작업을 최대한 병렬로 처리하기 위해 Signal만 걸어두고 tryFetch에서 대기
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues));
        m_lastSubmitFenceValues = m_fenceValues++;
        m_needsFetch = true;
    }
}

bool GPUTerrainBackend::tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates)
{
    if (!m_needsFetch) return false;
    m_needsFetch = false;
    
    if(m_fence->GetCompletedValue() < m_lastSubmitFenceValues)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_lastSubmitFenceValues, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
    }

    auto& r = m_rb[m_rbCursor];
    void* p = nullptr;
    CD3DX12_RANGE range(0, sizeof(UINT));
    r.rbCount->Map(0, &range, &p);
    memcpy(&r.count, p, sizeof(UINT));
    r.rbCount->Unmap(0, nullptr);
    if (r.count == 0)
    {
        return false;
    }

    {
        m_commandAllocator[m_rbCursor]->Reset();
        m_commandList->Reset(m_commandAllocator[m_rbCursor].Get(), nullptr);

        m_commandList->CopyBufferRegion(r.rbTriangles.Get(), 0, m_outBuffer.Get(), 0, r.count * sizeof(OutTriangle));

        m_commandList->Close();
        ID3D12CommandList* cmdList[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

        m_commandQueue->Signal(m_fence.Get(), m_fenceValues);
        m_fence->SetEventOnCompletion(m_fenceValues, m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
        ++m_fenceValues;
    }

    UINT triCount = r.count;
    // OutTriangle
    void* pTris = nullptr;
    D3D12_RANGE rrTri{ 0, (SIZE_T)triCount * sizeof(OutTriangle)};
    r.rbTriangles->Map(0, &rrTri, &pTris);
    const OutTriangle* OutTriangles = reinterpret_cast<const OutTriangle*>(pTris);
    
    std::vector<uint32_t> triPerChunk(m_numChunks, 0);
    for (uint32_t i = 0; i < triCount; ++i)
    {
        ++triPerChunk[OutTriangles[i].chunkIdx];
    }
    std::unordered_map<UINT, UINT> outchunkUpdatesTable;
    for (UINT i = 0; i < m_numChunks; ++i)
    {
        if (triPerChunk[i] == 0) continue;
        ChunkUpdate up;
        up.key = DecodeChunkKey(i, m_numChunkAxis);
        up.empty = false;
        up.md.vertices.clear();
        up.md.indices.clear();
        up.md.vertices.reserve(triPerChunk[i] * 3);
        up.md.indices.reserve(triPerChunk[i] * 3);
        up.md.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        OutChunkUpdates.push_back(up);
        outchunkUpdatesTable.insert_or_assign(i, OutChunkUpdates.size() - 1);
    }

    for (UINT i = 0; i < triCount; ++i)
    {
        const OutTriangle& tri = OutTriangles[i];
        UINT index = outchunkUpdatesTable[tri.chunkIdx];
        ChunkUpdate& up = OutChunkUpdates[index];

        UINT baseIndex = static_cast<UINT>(up.md.vertices.size());

        MeshData& md = up.md;
        md.vertices.push_back({ tri.A.position, tri.A.normal, {1,1,1,1} });
        md.vertices.push_back({ tri.B.position, tri.B.normal, {1,1,1,1} });
        md.vertices.push_back({ tri.C.position, tri.C.normal, {1,1,1,1} });

        md.indices.push_back(baseIndex + 0);
        md.indices.push_back(baseIndex + 1);
        md.indices.push_back(baseIndex + 2);
        baseIndex += 3;
    }
    r.rbTriangles->Unmap(0, nullptr);

    // 다음 프레임을 위한 링 advance
    m_rbCursor = (m_rbCursor + 1) % kRBFrameCount;
    m_ringCursor = (m_ringCursor + 1) % m_ring;
    // 이번 프레임에서 삭제 대기를 걸었던 리소스들 해제
    m_pendingDeletes.clear();

    return !OutChunkUpdates.empty();
}

void GPUTerrainBackend::ensureTriangleBuffer()
{
    if (m_outBuffer && m_outCounter) return;

    const UINT64 numElems = UINT64(m_numChunks) * UINT64(m_triCapPerChunk);
    const UINT64 total = numElems * sizeof(OutTriangle);
    
    MCUtil::CreateStructuredUavWithCounter(m_device, numElems, sizeof(OutTriangle), m_outBuffer, m_outCounter);
    NAME_D3D12_OBJECT(m_outBuffer);
    NAME_D3D12_OBJECT(m_outCounter);

}

void GPUTerrainBackend::computeNumChunks()
{
    const XMUINT3 totalCubes(m_grid.cells.x - 1, m_grid.cells.y - 1, m_grid.cells.z - 1);
 
    m_numChunkAxis = XMUINT3(
        (totalCubes.x + (s_chunkcubes - 1)) / s_chunkcubes,
        (totalCubes.y + (s_chunkcubes - 1)) / s_chunkcubes,
        (totalCubes.z + (s_chunkcubes - 1)) / s_chunkcubes
    );
    
    m_numChunks = m_numChunkAxis.x * m_numChunkAxis.y * m_numChunkAxis.z;
}

void GPUTerrainBackend::ensureRBSlot(UINT slot)
{
    auto& r = m_rb[slot];
    if (r.rbTriangles && r.rbCount) return;

    CD3DX12_HEAP_PROPERTIES hp_Readback(D3D12_HEAP_TYPE_READBACK);
    if (!r.rbTriangles)
    {
        const UINT64 bytes = UINT64(m_numChunks) * UINT64(m_triCapPerChunk) * sizeof(OutTriangle);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_Readback,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&r.rbTriangles)));
        NAME_D3D12_OBJECT(r.rbTriangles);
    }

    if (!r.rbCount)
    {
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(4);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_Readback,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&r.rbCount)));
        NAME_D3D12_OBJECT(r.rbCount);
    }
}

void GPUTerrainBackend::resetRBSlot(UINT slot)
{
    auto& r = m_rb[slot];
    r.rbCount.Reset();
    r.rbTriangles.Reset();
}