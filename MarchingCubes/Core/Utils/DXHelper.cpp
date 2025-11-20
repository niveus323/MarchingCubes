#include "pch.h"
#include "DXHelper.h"

std::wstring GetShaderFullPath(LPCWSTR shaderName)
{
    return GetFullPath(AssetType::Shader, shaderName);
}

HRESULT ReadDataFromFile(LPCWSTR filename, byte** data, uint32_t* size)
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

HRESULT ReadDataFromDDSFile(LPCWSTR filename, byte** data, uint32_t* offset, uint32_t* size)
{
    if (FAILED(ReadDataFromFile(filename, data, size)))
    {
        return E_FAIL;
    }

    // DDS files always start with the same magic number.
    static const uint32_t DDS_MAGIC = 0x20534444;
    uint32_t magicNumber = *reinterpret_cast<const uint32_t*>(*data);
    if (magicNumber != DDS_MAGIC)
    {
        return E_FAIL;
    }

    struct DDS_PIXELFORMAT
    {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t rgbBitCount;
        uint32_t rBitMask;
        uint32_t gBitMask;
        uint32_t bBitMask;
        uint32_t aBitMask;
    };

    struct DDS_HEADER
    {
        uint32_t size;
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        DDS_PIXELFORMAT ddsPixelFormat;
        uint32_t caps;
        uint32_t caps2;
        uint32_t caps3;
        uint32_t caps4;
        uint32_t reserved2;
    };

    auto ddsHeader = reinterpret_cast<const DDS_HEADER*>(*data + sizeof(uint32_t));
    if (ddsHeader->size != sizeof(DDS_HEADER) || ddsHeader->ddsPixelFormat.size != sizeof(DDS_PIXELFORMAT))
    {
        return E_FAIL;
    }

    const ptrdiff_t ddsDataOffset = sizeof(uint32_t) + sizeof(DDS_HEADER);
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
        uint32_t aligned = AlignCBSize(uint32_t(sizeBytes));
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
    void CreateAndUploadStructuredBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* data, uint32_t numElements, uint32_t stride, ComPtr<ID3D12Resource>& outDefault, ComPtr<ID3D12Resource>* outUpload = nullptr)
    {
        const uint64_t sizeBytes = uint64_t(numElements) * stride;

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
    void CreateStructuredUavWithCounter(ID3D12Device* device, uint32_t numElements, uint32_t stride, ComPtr<ID3D12Resource>& outBuffer, ComPtr<ID3D12Resource>& outCounter)
    {
        const uint64_t sizeBytes = uint64_t(numElements) * stride;

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
    void CreateOrUpdateDensity3D(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, uint32_t dimX, uint32_t dimY, uint32_t dimZ, const float* srcLinearXYZ, ComPtr<ID3D12Resource>& ioTex3D, ComPtr<ID3D12Resource>* outUpload = nullptr)
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

        const uint64_t rowPitch = uint64_t(dimX) * sizeof(float);
        const uint64_t slicePitch = rowPitch * dimY;

        D3D12_SUBRESOURCE_DATA s = {};
        s.pData = srcLinearXYZ;
        s.RowPitch = rowPitch;
        s.SlicePitch = slicePitch;

        const uint64_t uploadBytes = GetRequiredIntermediateSize(ioTex3D.Get(), 0, 1);
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
        const uint32_t kBytes = sizeof(uint32_t);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(kBytes);

        // 업로드 힙으로 만들고, 0을 써 둔다.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&g_zeroUpload)));
        NAME_D3D12_OBJECT(g_zeroUpload);

        // 채워넣기
        void* p = nullptr;
        D3D12_RANGE r{ 0, 0 }; // write-only
        ThrowIfFailed(g_zeroUpload->Map(0, &r, &p));
        *reinterpret_cast<uint32_t*>(p) = 0u;
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
        cmd->CopyBufferRegion(counter, 0, g_zeroUpload.Get(), 0, sizeof(uint32_t));

        // COPY_DEST -> after
        auto toAfter = CD3DX12_RESOURCE_BARRIER::Transition(
            counter, D3D12_RESOURCE_STATE_COPY_DEST, after);
        cmd->ResourceBarrier(1, &toAfter);
    }

}

namespace DescriptorHelper
{    
    void CreateSRV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU)
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

    void CreateSRV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU)
    {
        auto desc = res->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Buffer.FirstElement = 0;
        d.Buffer.NumElements = uint32_t(desc.Width / stride);
        d.Buffer.StructureByteStride = stride;
        d.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(res, &d, dstCPU);
    }

    void CreateUAV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter)
    {
        auto desc = res->GetDesc();
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        d.Buffer.FirstElement = 0;
        d.Buffer.NumElements = uint32_t(desc.Width / stride);
        d.Buffer.StructureByteStride = stride;
        d.Buffer.CounterOffsetInBytes = 0;
        d.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        device->CreateUnorderedAccessView(res, counter, &d, dstCPU);

    }

    void CreateUAV_Raw(ID3D12Device* device, ID3D12Resource* res, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, uint32_t firstElement, uint32_t numElements)
    {
        auto desc = res->GetDesc();
        if ((desc.Width % 4ull) != 0ull)
        {
            OutputDebugString(L"Raw Buffer must be 4-byte Aligned!!!!");
        }

        const uint32_t totalElemnts = static_cast<uint32_t>(desc.Width / 4ull);
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
}


namespace ConstantBufferHelper
{
    CBRing::CBRing(ID3D12Device* device, uint32_t ringCount, uint32_t bytesPerFrame) :
        m_ringCount(ringCount)
    {
        m_bytesPerFrame = AlignUp(bytesPerFrame, 256U);
        const uint32_t totalBytes = m_bytesPerFrame * ringCount;

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);
        auto hpUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&hpUpload, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_buffer.ReleaseAndGetAddressOf())));

        ThrowIfFailed(m_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cpu)));
        m_baseGPU = m_buffer->GetGPUVirtualAddress();
        m_headPerFrame.assign(ringCount, 0u);
    }

    D3D12_GPU_VIRTUAL_ADDRESS CBRing::PushBytes(uint32_t frameIndex, const void* src, uint32_t size)
    {
        const uint32_t n = AlignUp(size, 256u);
        uint32_t& head = m_headPerFrame[frameIndex];

        const uint32_t frameOffset = m_bytesPerFrame * frameIndex;
        std::memcpy(m_cpu + frameOffset + head, src, size);
        D3D12_GPU_VIRTUAL_ADDRESS va = m_baseGPU + frameOffset + head;
        head += n;

        return va;
    }

    uint32_t ConstantBufferHelper::CalcBytesPerFrame(const std::initializer_list<std::pair<uint32_t, uint32_t>> cbSizeAndCounts, float margin, uint32_t minFloor)
    {
        uint32_t raw = 0;
        for (const auto& item : cbSizeAndCounts)
        {
            uint32_t size = item.first;
            uint32_t countPerFrame = item.second;

            raw += AlignUp(size, 256u) * countPerFrame;
        }
        uint32_t bytesPerFrame = (uint32_t)(raw * margin);
        return std::max(AlignUp(bytesPerFrame, 256u), minFloor);
    }

    D3D12_GPU_VIRTUAL_ADDRESS PushOrSpill(ID3D12Device* device, CBRing& ring, uint32_t frameIdx, const void* src, uint32_t sizeBytes, std::vector<ComPtr<ID3D12Resource>>& pendingDeleteContainer)
    {
        const uint32_t need = AlignUp(sizeBytes, 256U);
        if (ring.Remaining(frameIdx) >= need)
            return ring.PushBytes(frameIdx, src, sizeBytes);

        ComPtr<ID3D12Resource> tmp;
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(need);
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(tmp.ReleaseAndGetAddressOf())));
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