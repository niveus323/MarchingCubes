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
    m_grid(gridDesc)
{
    m_vol = std::make_unique<SDFVolume3D>(device);
    m_brush = std::make_unique<GPUBrushCS>(device);
    m_mc = std::make_unique<GPUMarchingCubesCS>(device);

    UINT totalBytesPerFrame = ConstantBufferHelper::CalcBytesPerFrame({
            { sizeof(RegionArgsCBData), 1},
            { sizeof(BrushCBData), 1},
            { sizeof(GridCBData), 1}
        });

    m_cbRing = std::make_unique<ConstantBufferHelper::CBRing>(m_device, m_ring, totalBytesPerFrame);
    m_descriptorRing = std::make_unique<DescriptorHelper::DescriptorRing>(m_device, m_ring, kSlot_CountPerFrame, 1);
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
        m_rb[i].rbTriangles.Reset();
        ensureRBSlot(i);
    }
    ensureChunkMaskBuffer();
    ensureChunkMetaBuffer();
}

void GPUTerrainBackend::setFieldPtr(std::shared_ptr<_GRD> grid)
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
}

void GPUTerrainBackend::encode(ID3D12GraphicsCommandList* cmd)
{
    // encode가 발동하는 조건 : _GRD 갱신(m_fieldDirty == true), brush 사용(m_hasBrush == true)
    if (!m_device || (!m_fieldDirty && !m_hasBrush && !m_needsRemesh)) return;

    FrameAlloc fa{};
    fa.cbRing = m_cbRing.get();
    fa.descRing = m_descriptorRing.get();
    fa.ringCursor = m_ringCursor;

    initChunkBuffers(cmd, m_fieldDirty);

    if (m_fieldDirty)
    {
        m_vol->uploadFromGRD(cmd, m_gridData.get(), m_pendingDeletes);

        m_fieldDirty = false;
        m_needsRemesh = true;
    }

    SDFVolumeView volView{};
    volView.grid = m_grid;
    volView.tex = m_vol->density();
    volView.srv = m_descriptorRing->GpuAt(m_ringCursor, kSlot_t1);
    volView.uav = m_descriptorRing->GpuAt(m_ringCursor, kSlot_u1);

    // u5, u6 준비 (BrushRegionCS, MarchingCubesCS)
    {
        DescriptorHelper::CreateUAV_Structured(m_device, m_chunkMaskBuffer.Get(), sizeof(UINT), fa.descRing->CpuAt(fa.ringCursor, kSlot_u5));
        DescriptorHelper::CreateUAV_Structured(m_device, m_chunkMetaBuffer.Get(), sizeof(ChunkMeta), fa.descRing->CpuAt(fa.ringCursor, kSlot_u6));
    }

#if PIX_DEBUGMODE

    if (m_hasBrush && PIXGetCaptureState() != PIX_CAPTURE_GPU)
    {
        PIXCaptureParameters params = {};
        params.GpuCaptureParameters.FileName = L"Brush.wpix";
        PIXBeginCapture(PIX_CAPTURE_GPU, &params);
    }
    
#endif

    if (m_hasBrush)
    {
        GPUBrushEncodingContext context(
            m_device, 
            cmd, 
            volView, 
            fa, 
            m_requestedBrush, 
            s_chunkcubes
        );
        m_brush->encode(context);
        m_hasBrush = false;
    }

    // t0, t4, u0(+counter) 준비
    {
        DescriptorHelper::CreateSRV_Texture3D(m_device, m_vol->density(), DXGI_FORMAT_R32_FLOAT, fa.descRing->CpuAt(fa.ringCursor, kSlot_t1));
        DescriptorHelper::CreateSRV_Structured(m_device, m_chunkMaskBuffer.Get(), sizeof(UINT), fa.descRing->CpuAt(fa.ringCursor, kSlot_t3));
        DescriptorHelper::CreateUAV_Structured(m_device, m_outBuffer.Get(), sizeof(OutTriangle), m_descriptorRing->CpuAt(m_ringCursor, kSlot_u0), m_outCounter.Get());
    }
    
    if (m_needsRemesh)
    {
        auto trisToUav = CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &trisToUav);

        // u5 -> t4
        auto maskToSrv = CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMaskBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); // u5 -> t4
        cmd->ResourceBarrier(1, &maskToSrv);

        // 2) 카운터 리셋(0)
        MCUtil::ResetAndTransitCounter(m_device, cmd, m_outCounter.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        GPUMCEncodingContext context(
            m_device, 
            cmd, 
            volView, 
            fa, 
            m_requestedRemesh, 
            m_numChunkAxis, 
            s_chunkcubes, 
            m_triCapPerChunk
        );

        m_mc->encode(context);

        // readback 스케쥴
        const UINT rbSlot = m_rbCursor;
        ensureRBSlot(rbSlot);
        
        // Output Readback
        {
            CD3DX12_RESOURCE_BARRIER toCopySrc[3] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(m_outCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMetaBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
            };
            cmd->ResourceBarrier(_countof(toCopySrc), toCopySrc);

            const UINT64 totalBytes = UINT64(m_numChunks) * UINT64(m_triCapPerChunk) * sizeof(OutTriangle);
            cmd->CopyBufferRegion(m_rb[rbSlot].rbTriangles.Get(), 0, m_outBuffer.Get(), 0, totalBytes);  
            cmd->CopyBufferRegion(m_rb[rbSlot].rbCount.Get(), 0, m_outCounter.Get(), 0, 4);
            cmd->CopyBufferRegion(m_rb[rbSlot].rbMeta.Get(), 0, m_chunkMetaBuffer.Get(), 0, m_chunkMetaBytes);

            CD3DX12_RESOURCE_BARRIER backToUav[3] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(m_outCounter.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMetaBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            };
            cmd->ResourceBarrier(_countof(backToUav), backToUav);
        }        
        m_rb[rbSlot].scheduled = true;

        m_rbCursor = (m_rbCursor + 1) % kRBFrameCount;

        // 다음 프레임을 위한 링 advance (주의: frames-in-flight 이상이 되지 않도록 m_ring 조정)
        m_ringCursor = (m_ringCursor + 1) % m_ring;
        m_cbRing->BeginFrame(m_ringCursor);
        m_needsRemesh = false;
    }

#if PIX_DEBUGMODE
    if (PIXGetCaptureState() == PIX_CAPTURE_GPU)
    {
        PIXEndCapture(FALSE);
    }
#endif
}

bool GPUTerrainBackend::tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates)
{
    const UINT fetchSlot = (m_rbCursor + kRBFrameCount - 1) % kRBFrameCount;
    auto& r = m_rb[fetchSlot];
    if (!r.scheduled) return false; // 아직 삼각형 복사 스케줄이 안 됨

    // ChunkMeta (각 chunk에 대해 < 수정이 일어났는가, 수정 및 추가할 삼각형의 수>에 대한 데이터를 읽음)
    void* pMeta = nullptr;
    D3D12_RANGE rrMeta{ 0, (SIZE_T)(m_numChunks * kMetaStridebytes) };
    r.rbMeta->Map(0, &rrMeta, &pMeta);
    const uint8_t* metaBase = reinterpret_cast<const uint8_t*> (pMeta);

    // OutTriangle
    void* pTris = nullptr;
    const UINT64 totalElems = UINT64(m_numChunks) * UINT64(m_triCapPerChunk); 
    const UINT64 totalBytes = totalElems * sizeof(OutTriangle);
    D3D12_RANGE rrTri{ 0, (SIZE_T)totalBytes };
    r.rbTriangles->Map(0, &rrTri, &pTris);
    const OutTriangle* OutTriangles = reinterpret_cast<const OutTriangle*>(pTris);

    OutChunkUpdates.clear();
    OutChunkUpdates.reserve(m_numChunks);
    for (UINT idx = 0; idx < m_numChunks; ++idx)
    {
        const ChunkMeta* meta = reinterpret_cast<const ChunkMeta*>(metaBase + idx * sizeof(ChunkMeta));

        if (meta->touched == 0 && meta->counter == 0) continue;

        ChunkUpdate up{};
        up.key = DecodeChunkKey(idx, m_numChunkAxis);
        up.empty = (meta->counter == 0) ? true : false;

        if (!up.empty)
        {
            const UINT triCountChunk = meta->counter;
            const UINT64 base = UINT64(idx) * UINT64(m_triCapPerChunk);
            const OutTriangle* src = OutTriangles + base;

            MeshData& md = up.md;
            md.vertices.reserve(triCountChunk * 3);
            md.indices.reserve(triCountChunk * 3);

            UINT baseIndex = 0;
            for (UINT i = 0; i < triCountChunk; ++i)
            {
                const OutTriangle& tri = src[i];
                md.vertices.push_back({ tri.A.position, tri.A.normal, {1,1,1,1} });
                md.vertices.push_back({ tri.B.position, tri.B.normal, {1,1,1,1} });
                md.vertices.push_back({ tri.C.position, tri.C.normal, {1,1,1,1} });

                md.indices.push_back(baseIndex + 0);
                md.indices.push_back(baseIndex + 1);
                md.indices.push_back(baseIndex + 2);
                baseIndex += 3;
            }
            md.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
        OutChunkUpdates.push_back(std::move(up));
    }
    r.rbMeta->Unmap(0, nullptr);
    r.rbTriangles->Unmap(0, nullptr);
    r.scheduled = false;

    return !OutChunkUpdates.empty();
}

void GPUTerrainBackend::drainKeepAlive(std::vector<ComPtr<ID3D12Resource>>& dst)
{
    dst.insert(dst.end(), std::make_move_iterator(m_pendingDeletes.begin()), std::make_move_iterator(m_pendingDeletes.end()));
    m_pendingDeletes.clear();
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

void GPUTerrainBackend::ensureChunkMaskBuffer()
{
    XMUINT3 totalcubes = XMUINT3{ m_grid.cells.x - 1u, m_grid.cells.y - 1u, m_grid.cells.z - 1u };

    const XMUINT3 numChunksAxis = XMUINT3{
        (totalcubes.x + (s_chunkcubes - 1)) / s_chunkcubes,
        (totalcubes.y + (s_chunkcubes - 1)) / s_chunkcubes,
        (totalcubes.z + (s_chunkcubes - 1)) / s_chunkcubes
    }; // 7,7,7
    const UINT total = numChunksAxis.x * numChunksAxis.y * numChunksAxis.z; // 343
    const UINT words = (total + 31u) / 32u; // 11
    const UINT bytes = words * 4u; // 44
    const UINT lastBits = (total & 31u) ? (total & 31u) : 32u;

    if (m_chunkMaskBuffer && m_chunkMaskBytes == bytes) return;
    m_chunkMaskBytes = bytes;

    // default 버퍼 생성
    {
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto hp_default = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_default,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_chunkMaskBuffer)
        ));
        NAME_D3D12_OBJECT(m_chunkMaskBuffer);
    }

    // upload 버퍼 생성
    {
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        auto hp_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        // 0으로 채우는 Upload 버퍼
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_upload,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_chunkMaskUpload_Zeros)
        ));
        NAME_D3D12_OBJECT(m_chunkMaskUpload_Zeros);

        void* p = nullptr;
        CD3DX12_RANGE r(0, 0);
        m_chunkMaskUpload_Zeros->Map(0, &r, &p);
        std::memset(p, 0, bytes);
        m_chunkMaskUpload_Zeros->Unmap(0, nullptr);

        // 1로 채우는 Upload 버퍼
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_upload,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_chunkMaskUpload_Ones)
        ));
        NAME_D3D12_OBJECT(m_chunkMaskUpload_Ones);

        uint32_t* w = nullptr;
        m_chunkMaskUpload_Ones->Map(0, nullptr, (void**)&w);
        for (UINT i = 0; i < words; ++i) w[i] = 0xFFFFFFFFu;
        if (lastBits < 32u) w[words - 1] = (lastBits == 0) ? 0u : ((1u << lastBits) - 1u);
        m_chunkMaskUpload_Ones->Unmap(0, nullptr);
    }
}

void GPUTerrainBackend::ensureChunkMetaBuffer()
{
    XMUINT3 totalcubes = XMUINT3{ m_grid.cells.x - 1u, m_grid.cells.y - 1u, m_grid.cells.z - 1u };
    
    const XMUINT3 numChunksAxis = XMUINT3{
        (totalcubes.x + (s_chunkcubes - 1)) / s_chunkcubes,
        (totalcubes.y + (s_chunkcubes - 1)) / s_chunkcubes,
        (totalcubes.z + (s_chunkcubes - 1)) / s_chunkcubes
    };
    const UINT total = numChunksAxis.x * numChunksAxis.y * numChunksAxis.z;
    const UINT bytes = total * kMetaStridebytes; // pred64(8) + counter(4) + padding(4)

    if (m_chunkMetaBuffer && m_chunkMetaBytes == bytes) return;

    m_chunkMetaBytes = bytes;

    if (!m_chunkMetaBuffer)
    {
        CD3DX12_HEAP_PROPERTIES hp_default(D3D12_HEAP_TYPE_DEFAULT);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_default,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_chunkMetaBuffer)));
        NAME_D3D12_OBJECT(m_chunkMetaBuffer);
    }

    if (!m_chunkMetaUpload_Zeros)
    {
        CD3DX12_HEAP_PROPERTIES hp_upload(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_upload,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_chunkMetaUpload_Zeros)));
        NAME_D3D12_OBJECT(m_chunkMetaUpload_Zeros);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_upload,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_chunkMetaUpload_Ones)));
        NAME_D3D12_OBJECT(m_chunkMetaUpload_Ones);

        uint8_t* p1 = nullptr;
        uint8_t* p2 = nullptr;
        m_chunkMetaUpload_Zeros->Map(0, nullptr, (void**) & p1);
        m_chunkMetaUpload_Ones->Map(0, nullptr, (void**)&p2); 
        std::memset(p1, 0, bytes);
        for (UINT off = 0; off < bytes; off += sizeof(ChunkMeta))
        {
            auto* chunkMeta = reinterpret_cast<ChunkMeta*>(p2 + off);
            chunkMeta->touched = 1;
            chunkMeta->counter = 0;
        }
        m_chunkMetaUpload_Zeros->Unmap(0, nullptr);
        m_chunkMetaUpload_Ones->Unmap(0, nullptr);
    }
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
    if (r.rbMeta && r.rbTriangles && r.rbCount) return;

    CD3DX12_HEAP_PROPERTIES hp_Readback(D3D12_HEAP_TYPE_READBACK);
    if (!r.rbMeta)
    {
        const UINT bytes = m_numChunks * 16; // pred64(8) + counter(4) + padding(4)
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp_Readback,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&r.rbMeta)));
        NAME_D3D12_OBJECT(r.rbMeta);
    }

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
    r.scheduled = false;
}

void GPUTerrainBackend::resetRBSlot(UINT slot)
{
    auto& r = m_rb[slot];
    r.rbMeta.Reset();
    r.rbTriangles.Reset();
}

void GPUTerrainBackend::initChunkBuffers(ID3D12GraphicsCommandList* cmd, bool bMarkAllDitry)
{
    ensureChunkMaskBuffer();
    ensureChunkMetaBuffer();

    // m_chunkMaskBuffer 0으로 클리어
    {
        auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMaskBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->ResourceBarrier(1, &toCopy);
        ID3D12Resource* src = bMarkAllDitry ? m_chunkMaskUpload_Ones.Get() : m_chunkMaskUpload_Zeros.Get();
        cmd->CopyBufferRegion(m_chunkMaskBuffer.Get(), 0, src, 0, m_chunkMaskBytes);
        auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMaskBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &toUav);
    }

    // m_chunkMetaBuffer 0로 클리어
    {
        auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMetaBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->ResourceBarrier(1, &toCopy);
        ID3D12Resource* src = bMarkAllDitry ? m_chunkMetaUpload_Ones.Get() : m_chunkMetaUpload_Zeros.Get();
        cmd->CopyBufferRegion(m_chunkMetaBuffer.Get(), 0, src, 0, m_chunkMetaBytes);
        auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(m_chunkMetaBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &toUav);
    }


}
