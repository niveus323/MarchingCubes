#include "pch.h"
#include "DXHelper.h"


std::wstring GetShaderFullPath(LPCWSTR shaderName)
{
    return GetFullPath(AssetType::Shader, shaderName);
}

HRESULT ReadDataFromFile(LPCWSTR filename, byte** data, UINT* size)
{
    using namespace Microsoft::WRL;

#if WINVER >= _WIN32_WINNT_WIN8
    CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
    extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
    extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
    extendedParams.lpSecurityAttributes = nullptr;
    extendedParams.hTemplateFile = nullptr;

    Wrappers::FileHandle file(CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extendedParams));
#else
    Wrappers::FileHandle file(CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr));
#endif
    if (file.Get() == INVALID_HANDLE_VALUE)
    {
        throw std::exception();
    }

    FILE_STANDARD_INFO fileInfo = {};
    if (!GetFileInformationByHandleEx(file.Get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        throw std::exception();
    }

    if (fileInfo.EndOfFile.HighPart != 0)
    {
        throw std::exception();
    }

    *data = reinterpret_cast<byte*>(malloc(fileInfo.EndOfFile.LowPart));
    *size = fileInfo.EndOfFile.LowPart;

    if (!ReadFile(file.Get(), *data, fileInfo.EndOfFile.LowPart, nullptr, nullptr))
    {
        throw std::exception();
    }

    return S_OK;
}

HRESULT ReadDataFromDDSFile(LPCWSTR filename, byte** data, UINT* offset, UINT* size)
{
    if (FAILED(ReadDataFromFile(filename, data, size)))
    {
        return E_FAIL;
    }

    // DDS files always start with the same magic number.
    static const UINT DDS_MAGIC = 0x20534444;
    UINT magicNumber = *reinterpret_cast<const UINT*>(*data);
    if (magicNumber != DDS_MAGIC)
    {
        return E_FAIL;
    }

    struct DDS_PIXELFORMAT
    {
        UINT size;
        UINT flags;
        UINT fourCC;
        UINT rgbBitCount;
        UINT rBitMask;
        UINT gBitMask;
        UINT bBitMask;
        UINT aBitMask;
    };

    struct DDS_HEADER
    {
        UINT size;
        UINT flags;
        UINT height;
        UINT width;
        UINT pitchOrLinearSize;
        UINT depth;
        UINT mipMapCount;
        UINT reserved1[11];
        DDS_PIXELFORMAT ddsPixelFormat;
        UINT caps;
        UINT caps2;
        UINT caps3;
        UINT caps4;
        UINT reserved2;
    };

    auto ddsHeader = reinterpret_cast<const DDS_HEADER*>(*data + sizeof(UINT));
    if (ddsHeader->size != sizeof(DDS_HEADER) || ddsHeader->ddsPixelFormat.size != sizeof(DDS_PIXELFORMAT))
    {
        return E_FAIL;
    }

    const ptrdiff_t ddsDataOffset = sizeof(UINT) + sizeof(DDS_HEADER);
    *offset = ddsDataOffset;
    *size = *size - ddsDataOffset;

    return S_OK;
}

namespace MCUtil
{
    static ComPtr<ID3D12Resource> g_zeroUpload;

    // 업로드 힙 상수버퍼 (즉시 Map/memcpy) 생성 함수
    void CreateUploadConstantBuffer(ID3D12Device* device, const void* data, size_t sizeBytes, ComPtr<ID3D12Resource>& outUpload)
    {
        UINT aligned = AlignCBSize(UINT(sizeBytes));
        CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(aligned);

        ThrowIfFailed(device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(outUpload.ReleaseAndGetAddressOf())));

        if (data && sizeBytes) {
            void* p = nullptr;
            CD3DX12_RANGE range(0, 0);
            ThrowIfFailed(outUpload->Map(0, &range, &p));
            std::memcpy(p, data, sizeBytes);
            outUpload->Unmap(0, nullptr);
        }
    }

    // Defaul 버퍼 + 업로드 copy (data가 nullptr이면 빈 버퍼만 생성)
    void CreateAndUploadStructuredBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* data, UINT numElements, UINT stride, ComPtr<ID3D12Resource>& outDefault, ComPtr<ID3D12Resource>* outUpload = nullptr)
    {
        const UINT64 sizeBytes = UINT64(numElements) * stride;

        // Default buffer (COPY_DEST)
        CD3DX12_HEAP_PROPERTIES hpDef(D3D12_HEAP_TYPE_DEFAULT);
        auto descDef = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_NONE);
        ThrowIfFailed(device->CreateCommittedResource(
            &hpDef, D3D12_HEAP_FLAG_NONE, &descDef,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(outDefault.ReleaseAndGetAddressOf())));

        if (data && sizeBytes) {
            // Upload buffer
            ComPtr<ID3D12Resource> upload;
            CD3DX12_HEAP_PROPERTIES hpUp(D3D12_HEAP_TYPE_UPLOAD);
            auto descUp = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes);
            ThrowIfFailed(device->CreateCommittedResource(
                &hpUp, D3D12_HEAP_FLAG_NONE, &descUp,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(upload.ReleaseAndGetAddressOf())));

            void* p = nullptr; CD3DX12_RANGE r(0, 0);
            ThrowIfFailed(upload->Map(0, &r, &p));
            std::memcpy(p, data, sizeBytes);
            upload->Unmap(0, nullptr);

            auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
                outDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
            cmdList->ResourceBarrier(1, &toCopyDest);

            cmdList->CopyBufferRegion(outDefault.Get(), 0, upload.Get(), 0, sizeBytes);

            if (outUpload) *outUpload = upload;
        }
    }

    // AppendStructuredUAV 버퍼 + Counter 버퍼 생성
    void CreateStructuredUavWithCounter(ID3D12Device* device, UINT numElements, UINT stride, ComPtr<ID3D12Resource>& outBuffer, ComPtr<ID3D12Resource>& outCounter)
    {
        const UINT64 sizeBytes = UINT64(numElements) * stride;

        CD3DX12_HEAP_PROPERTIES hpDef(D3D12_HEAP_TYPE_DEFAULT);

        auto descUav = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(device->CreateCommittedResource(
            &hpDef, D3D12_HEAP_FLAG_NONE, &descUav,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(outBuffer.ReleaseAndGetAddressOf())));

        auto descCnt = CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(device->CreateCommittedResource(
            &hpDef, D3D12_HEAP_FLAG_NONE, &descCnt,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(outCounter.ReleaseAndGetAddressOf())));
    }

    // 카운터 값을 Readback 버퍼로 복사 (4-Bytes)
    void CopyCounterToReadback(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* counter, ID3D12Resource* readback4B)
    {
        // counter(4B) → readback
        cmdList->CopyBufferRegion(readback4B, 0, counter, 0, 4);
    }

    // 버퍼 내용을 Readback 버퍼로 복사
    void CopyBufferToReadback(ID3D12GraphicsCommandList* cmd, ID3D12Resource* src, ID3D12Resource* readback, size_t bytes)
    {
        cmd->CopyBufferRegion(readback, 0, src, 0, bytes);
    }

    // 3D Density Field 유틸리티

    // 3D Density Field 초기화 (SRV/UAV 디스크립터는 별도 생성)
    void CreateOrUpdateDensity3D(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, UINT dimX, UINT dimY, UINT dimZ, const float* srcLinearXYZ, ComPtr<ID3D12Resource>& ioTex3D, ComPtr<ID3D12Resource>* outUpload = nullptr)
    {
        const DXGI_FORMAT fmt = DXGI_FORMAT_R32_FLOAT;

        bool needCreate = true;
        if (ioTex3D) {
            auto desc = ioTex3D->GetDesc();
            needCreate = !(desc.Width == dimX && desc.Height == dimY &&
                desc.DepthOrArraySize == dimZ && desc.Format == fmt);
        }

        if (needCreate) {
            D3D12_RESOURCE_DESC texDesc = {};
            texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            texDesc.Width = dimX;
            texDesc.Height = dimY;
            texDesc.DepthOrArraySize = (UINT16)dimZ;
            texDesc.MipLevels = 1;
            texDesc.Format = fmt;
            texDesc.SampleDesc = { 1, 0 };
            texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            CD3DX12_HEAP_PROPERTIES hpDef(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(device->CreateCommittedResource(
                &hpDef, D3D12_HEAP_FLAG_NONE, &texDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(ioTex3D.ReleaseAndGetAddressOf())));
        }
        else {
            auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
                ioTex3D.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
            cmd->ResourceBarrier(1, &toCopy);
        }

        const UINT64 rowPitch = UINT64(dimX) * sizeof(float);
        const UINT64 slicePitch = rowPitch * dimY;

        D3D12_SUBRESOURCE_DATA s = {};
        s.pData = srcLinearXYZ;
        s.RowPitch = rowPitch;
        s.SlicePitch = slicePitch;

        const UINT64 uploadBytes = GetRequiredIntermediateSize(ioTex3D.Get(), 0, 1);
        ComPtr<ID3D12Resource> upload;
        CD3DX12_HEAP_PROPERTIES hpUp(D3D12_HEAP_TYPE_UPLOAD);
        auto descUp = CD3DX12_RESOURCE_DESC::Buffer(uploadBytes);
        ThrowIfFailed(device->CreateCommittedResource(
            &hpUp, D3D12_HEAP_FLAG_NONE, &descUp,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(upload.ReleaseAndGetAddressOf())));

        UpdateSubresources(cmd, ioTex3D.Get(), upload.Get(), 0, 0, 1, &s);

        if (outUpload) *outUpload = upload;

        auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
            ioTex3D.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &toSrv);
    }

    void EnsureZeroUpload(ID3D12Device* device)
    {
        if (g_zeroUpload) return;

        // 4바이트만 있어도 충분하지만, Buffer 최소 정렬은 크게 요구되지 않음(여기선 4B)
        const UINT kBytes = sizeof(UINT);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(kBytes);

        // 업로드 힙으로 만들고, 0을 써 둔다.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&g_zeroUpload)));

        // 채워넣기
        void* p = nullptr;
        D3D12_RANGE r{ 0, 0 }; // write-only
        ThrowIfFailed(g_zeroUpload->Map(0, &r, &p));
        *reinterpret_cast<UINT*>(p) = 0u;
        g_zeroUpload->Unmap(0, nullptr);
    }

    void ResetAndTransitCounter(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, ID3D12Resource* counter, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        // 4바이트 0 업로드 버퍼 확보(내부에서 1회 생성 후 재사용)
        EnsureZeroUpload(device);

        // before -> COPY_DEST
        if (before != D3D12_RESOURCE_STATE_COPY_DEST) {
            auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
                counter, before, D3D12_RESOURCE_STATE_COPY_DEST);
            cmd->ResourceBarrier(1, &toCopyDest);
        }

        // 0을 카운터 리소스의 시작(오프셋 0)에 복사
        cmd->CopyBufferRegion(counter, 0, g_zeroUpload.Get(), 0, sizeof(UINT));

        // COPY_DEST -> after
        auto toAfter = CD3DX12_RESOURCE_BARRIER::Transition(
            counter, D3D12_RESOURCE_STATE_COPY_DEST, after);
        cmd->ResourceBarrier(1, &toAfter);
    }

}

namespace DescriptorHelper
{    
    DescriptorHelper::DescriptorRing::DescriptorRing(ID3D12Device* device, UINT ringCount, UINT descriptorsPerFrame, UINT staticCount) :
        m_ringCount(ringCount),
        m_perFrame(descriptorsPerFrame),
        m_staticCount(staticCount)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = m_staticCount + descriptorsPerFrame * m_ringCount;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));
        NAME_D3D12_OBJECT(m_heap);
        
        m_inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_cpuBase = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_gpuBase = m_heap->GetGPUDescriptorHandleForHeapStart();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorRing::GpuAt(UINT frameIdx, UINT slot) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE h = m_gpuBase;
        h.ptr += SIZE_T((m_staticCount + (frameIdx * m_perFrame) + slot) * m_inc);
        return h;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorRing::StaticGpuAt(UINT index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE h = m_gpuBase;
        h.ptr += SIZE_T(index * m_inc);
        return h;
    }
    
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorRing::CpuAt(UINT frameIdx, UINT slot) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE h = m_cpuBase;
        h.ptr += SIZE_T((m_staticCount + (frameIdx * m_perFrame) + slot) * m_inc);
        return h;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorRing::StaticCpuAt(UINT index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE h = m_cpuBase;
        h.ptr += SIZE_T(index * m_inc);
        return h;
    }

    void CopyToFrameSlot(ID3D12Device* device, DescriptorRing& ring, UINT frameIdx, UINT slot, D3D12_CPU_DESCRIPTOR_HANDLE srcCPU)
    {
        auto dst = ring.CpuAt(frameIdx, slot);
        device->CopyDescriptorsSimple(1, dst, srcCPU, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    void DescriptorHelper::CopyRange(ID3D12Device* device, DescriptorRing& ring, UINT frameIdx, UINT baseSlot, const D3D12_CPU_DESCRIPTOR_HANDLE* srcCPU, UINT count)
    {
        for (UINT i = 0; i < count; ++i)
        {
            CopyToFrameSlot(device, ring, frameIdx, baseSlot + i, srcCPU[i]);
        }
    }

    void DescriptorHelper::CreateSRV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format = format;
        d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        auto desc = res->GetDesc();
        d.Texture3D.MipLevels = 1;
        d.Texture3D.MostDetailedMip = 0;
        d.Texture3D.ResourceMinLODClamp = 0.0f;
        device->CreateShaderResourceView(res, &d, dstCPU);
    }

    void CreateUAV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_R32_FLOAT;
        d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        d.Texture3D.MipSlice = 0;
        d.Texture3D.FirstWSlice = 0;
        d.Texture3D.WSize = res->GetDesc().DepthOrArraySize;
        device->CreateUnorderedAccessView(res, counter, &d, dstCPU);
    }

    void CreateSRV_Structured(ID3D12Device* device, ID3D12Resource* res, UINT stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU)
    {
        auto desc = res->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Buffer.FirstElement = 0;
        d.Buffer.NumElements = UINT(desc.Width / stride);
        d.Buffer.StructureByteStride = stride;
        d.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(res, &d, dstCPU);
    }

    void CreateUAV_Structured(ID3D12Device* device, ID3D12Resource* res, UINT stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter)
    {
        auto desc = res->GetDesc();
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        d.Buffer.FirstElement = 0;
        d.Buffer.NumElements = UINT(desc.Width / stride);
        d.Buffer.StructureByteStride = stride;
        d.Buffer.CounterOffsetInBytes = 0;
        d.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        device->CreateUnorderedAccessView(res, counter, &d, dstCPU);

    }

    void DescriptorHelper::CreateUAV_Raw(ID3D12Device* device, ID3D12Resource* res, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, UINT firstElement, UINT numElements)
    {
        auto desc = res->GetDesc();
        if ((desc.Width % 4ull) != 0ull)
        {
            OutputDebugString(L"Raw Buffer must be 4-byte Aligned!!!!");
        }

        const UINT totalElemnts = static_cast<UINT>(desc.Width / 4ull);
        if (numElements == 0)
        {
            numElements = totalElemnts - firstElement;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_R32_TYPELESS;
        d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        d.Buffer.FirstElement = firstElement;
        d.Buffer.NumElements = numElements;
        d.Buffer.StructureByteStride = 0;
        d.Buffer.CounterOffsetInBytes = 0;
        d.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        
        device->CreateUnorderedAccessView(res, nullptr, &d, dstCPU);

    }

    void DescriptorHelper::SetTable(ID3D12GraphicsCommandList* cmd, DescriptorRing& ring, UINT frameIdx, std::initializer_list<std::pair<UINT,UINT>> paramAndSlots)
    {
        for (const auto& item : paramAndSlots)
        {
            UINT paramIdx = item.first;
            UINT slotIdx = item.second;

            cmd->SetComputeRootDescriptorTable(paramIdx, ring.GpuAt(frameIdx, slotIdx));

        }        
    }

}


namespace ConstantBufferHelper
{
    CBRing::CBRing(ID3D12Device* device, UINT ringCount, UINT bytesPerFrame) :
        m_ringCount(ringCount)
    {
        m_bytesPerFrame = AlignUp(bytesPerFrame, 256U);
        const UINT totalBytes = m_bytesPerFrame * ringCount;

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);
        auto hpUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&hpUpload, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_buffer)));

        ThrowIfFailed(m_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cpu)));
        m_baseGPU = m_buffer->GetGPUVirtualAddress();
        m_headPerFrame.assign(ringCount, 0u);
    }

    D3D12_GPU_VIRTUAL_ADDRESS CBRing::PushBytes(UINT frameIndex, const void* src, UINT size)
    {
        const UINT n = AlignUp(size, 256u);
        UINT& head = m_headPerFrame[frameIndex];

        const UINT frameOffset = m_bytesPerFrame * frameIndex;
        std::memcpy(m_cpu + frameOffset + head, src, size);
        D3D12_GPU_VIRTUAL_ADDRESS va = m_baseGPU + frameOffset + head;
        head += n;

        return va;
    }

    UINT ConstantBufferHelper::CalcBytesPerFrame(const std::initializer_list<std::pair<UINT, UINT>> cbSizeAndCounts, float margin, UINT minFloor)
    {
        UINT raw = 0;
        for (const auto& item : cbSizeAndCounts)
        {
            UINT size = item.first;
            UINT countPerFrame = item.second;

            raw += AlignUp(size, 256u) * countPerFrame;
        }
        UINT bytesPerFrame = (UINT)(raw * margin);
        return std::max(AlignUp(bytesPerFrame, 256u), minFloor);
    }

    D3D12_GPU_VIRTUAL_ADDRESS PushOrSpill(ID3D12Device* device, CBRing& ring, UINT frameIdx, const void* src, UINT sizeBytes, std::vector<ComPtr<ID3D12Resource>>& pendingDeleteContainer)
    {
        const UINT need = AlignUp(sizeBytes, 256U);
        if (ring.Remaining(frameIdx) >= need)
            return ring.PushBytes(frameIdx, src, sizeBytes);

        ComPtr<ID3D12Resource> tmp;
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(need);
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&tmp)));
        void* p = nullptr;
        ThrowIfFailed(tmp->Map(0, nullptr, &p));
        std::memcpy(p, src, sizeBytes);
        tmp->Unmap(0, nullptr);
        pendingDeleteContainer.push_back(tmp);
        auto va = tmp->GetGPUVirtualAddress();
        assert((va & 0xFFu) == 0 && "CBV must be 256B aligned");
        return va;

    }
}