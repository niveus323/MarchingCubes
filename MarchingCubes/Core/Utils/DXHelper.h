#pragma once
#include <stdexcept>
#include <DirectXMath.h>
#include <span>
#include <Core/Geometry/UploadRing.h>

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;
using namespace DirectX;

extern UploadRing* g_uploadRing;

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

inline void ThrowIfFalse(bool result, const char* msg)
{
    if (!result)
        throw std::runtime_error(msg);
}

enum class AssetType 
{ 
    Default = 0,
    Texture, 
    Shader, 
    Audio, 
    Data, 
};

inline std::wstring GetFullPath(AssetType type, LPCWSTR name)
{
    const wchar_t* root = nullptr;
    switch (type)
    {
    case AssetType::Texture: root = ASSETS_ROOT; break;
    case AssetType::Shader:  root = SHADERS_ROOT; break;
    default:                  root = ASSETS_ROOT; break;
    }
    return std::wstring(root) + L"/" + std::wstring(name);
}

std::wstring GetShaderFullPath(LPCWSTR shaderName);

HRESULT ReadDataFromFile(LPCWSTR filename, byte** data, UINT* size);

HRESULT ReadDataFromDDSFile(LPCWSTR filename, byte** data, UINT* offset, UINT* size);

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
inline void SetNameAlias(ID3D12Object* pObject, LPCWSTR name, LPCWSTR alias)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s(%s)", name, alias) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
inline void SetNameAlias(ID3D12Object*, LPCWSTR, LPCWSTR)
{
}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)
#define NAME_D3D12_OBJECT_ALIAS(x, s) SetNameAlias((x).Get(), L#x, s)
#define NAME_D3D12_OBJECT_ALIAS_INDEXED(x, n, s) SetNameAlias((x)[n].Get(), L#x, s);

inline UINT CalculateConstantBufferByteSize(UINT byteSize)
{
    // Constant buffer size is required to be aligned.
    return (byteSize + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}

#ifdef D3D_COMPILE_STANDARD_FILE_INCLUDE
inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr;

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}
#endif

// Resets all elements in a ComPtr array.
template<class T>
void ResetComPtrArray(T* comPtrArray)
{
    for (auto& i : *comPtrArray)
    {
        i.Reset();
    }
}


// Resets all elements in a unique_ptr array.
template<class T>
void ResetUniquePtrArray(T* uniquePtrArray)
{
    for (auto& i : *uniquePtrArray)
    {
        i.reset();
    }
}

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define PIPELINEMODE_LIST \
    X(Filled)             \
    X(Line)               \
    X(Wire)

enum class PipelineMode
{
#define X(name) name,
    PIPELINEMODE_LIST
#undef X 
    Count
};

inline LPCWSTR ToLPCWSTR(PipelineMode mode)
{
    switch (mode)
    {
#define X(name) case PipelineMode::name: return L#name;
        PIPELINEMODE_LIST
#undef X
    default:
        return L"<Unknown PipelineMode>";
    }
}

inline XMVECTOR ToQuatFromEuler(const XMFLOAT3& euler)
{
    return XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z);
}

inline XMFLOAT3 ToEulerFromQuat(const XMVECTOR& quat)
{
    XMFLOAT3 euler;

    // 회전 행렬로 변환
    XMMATRIX R = XMMatrixRotationQuaternion(quat);

    // 행렬을 row-major로 디컴포즈
    float pitch, yaw, roll;

    // 참고: XMMatrix는 row-major 기준임
    pitch = asinf(-R.r[2].m128_f32[1]);  // -m13
    if (cosf(pitch) > 1e-6f)
    {
        yaw = atan2f(R.r[2].m128_f32[0], R.r[2].m128_f32[2]);  // m11, m33
        roll = atan2f(R.r[0].m128_f32[1], R.r[1].m128_f32[1]); // m21, m22
    }
    else
    {
        // Gimbal lock 발생 시
        yaw = 0.0f;
        roll = atan2f(-R.r[1].m128_f32[0], R.r[0].m128_f32[0]); // -m21, m11
    }

    euler = XMFLOAT3(pitch, yaw, roll); // 라디안 단위
    return euler;
}

static UINT AlignUp(UINT size, UINT align)
{
    return (size + align - 1) & ~(align - 1);
}

static UINT64 AlignUp64(UINT64 size, UINT64 align)
{
    return (size + align - 1) & ~(align - 1);
}

namespace MCUtil {
    inline UINT AlignCBSize(UINT size)
    {
        return AlignUp(size, 256u);
    }

    // 업로드 힙 상수버퍼 (즉시 Map/memcpy) 생성 함수
    void CreateUploadConstantBuffer(ID3D12Device* device, const void* data, size_t sizeBytes, ComPtr<ID3D12Resource>& outUpload);

    // Defaul 버퍼 + 업로드 copy (data가 nullptr이면 빈 버퍼만 생성)
    void CreateAndUploadStructuredBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* data, UINT numElements, UINT stride, ComPtr<ID3D12Resource>& outDefault, ComPtr<ID3D12Resource>* outUpload);

    // AppendStructuredUAV 버퍼 + Counter 버퍼 생성
    void CreateStructuredUavWithCounter(ID3D12Device* device, UINT numElements, UINT stride, ComPtr<ID3D12Resource>& outBuffer, ComPtr<ID3D12Resource>& outCounter);

    // 카운터 값을 Readback 버퍼로 복사 (4-Bytes)
    void CopyCounterToReadback(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* counter, ID3D12Resource* readback4B);

    // 버퍼 내용을 Readback 버퍼로 복사
    void CopyBufferToReadback(ID3D12GraphicsCommandList* cmd, ID3D12Resource* src, ID3D12Resource* readback, size_t bytes);

    // 3D Density Field 유틸리티

    // 3D Density Field 초기화 (SRV/UAV 디스크립터는 별도 생성)
    void CreateOrUpdateDensity3D(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, UINT dimX, UINT dimY, UINT dimZ, const float* srcLinearXYZ, ComPtr<ID3D12Resource>& ioTex3D, ComPtr<ID3D12Resource>* outUpload);

    static void EnsureZeroUpload(ID3D12Device* device);

    // Append Counter 0으로 리셋
    void ResetAndTransitCounter(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, ID3D12Resource* counter, D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

namespace DescriptorHelper
{
    class DescriptorRing
    {
    public:
        DescriptorRing(ID3D12Device* device, UINT ringCount, UINT descriptorsPerFrame, UINT staticCount);

        // 동적 GPU Descriptor Handle 위치 인덱싱
        D3D12_GPU_DESCRIPTOR_HANDLE GpuAt(UINT frameIdx, UINT slot) const;
        // 정적 GPU Descriptor Handle 위치 인덱싱
        D3D12_GPU_DESCRIPTOR_HANDLE StaticGpuAt(UINT index = 0) const;

        // 동적 CPU Descriptor Handle 위치 인덱싱
        D3D12_CPU_DESCRIPTOR_HANDLE CpuAt(UINT frameIdx, UINT slot) const;
        D3D12_CPU_DESCRIPTOR_HANDLE StaticCpuAt(UINT index = 0) const;

        ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
        UINT Inc() const { return m_inc; }
        UINT SlotsPerFrame() const { return m_perFrame; }
        UINT RingCount() const { return m_ringCount; }

    private:

        ComPtr<ID3D12DescriptorHeap>  m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE  m_cpuBase{};
        D3D12_GPU_DESCRIPTOR_HANDLE  m_gpuBase{};
        UINT m_inc = 0;
        UINT m_perFrame = 0;
        UINT m_ringCount = 0;
        UINT m_staticCount = 0;
    };

    void CopyToFrameSlot(ID3D12Device* device, DescriptorRing& ring, UINT frameIdx, UINT slot, D3D12_CPU_DESCRIPTOR_HANDLE srcCPU);
    void CopyRange(ID3D12Device* device, DescriptorRing& ring, UINT frameIdx, UINT baseSlot, const D3D12_CPU_DESCRIPTOR_HANDLE* srcCPU, UINT count);

    void CreateSRV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU);
    void CreateUAV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter = nullptr);
    void CreateSRV_Structured(ID3D12Device* device, ID3D12Resource* res, UINT stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU);
    void CreateUAV_Structured(ID3D12Device* device, ID3D12Resource* res, UINT stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter = nullptr);
    void CreateUAV_Raw(ID3D12Device* device, ID3D12Resource* res, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, UINT firstElement = 0, UINT numElements = 0);

    void SetTable(ID3D12GraphicsCommandList* cmd, DescriptorRing& ring, UINT frameIdx, std::initializer_list<std::pair<UINT,UINT>> paramAndSlots);
    inline void AssertContiguousSlots(UINT a, UINT b) // 연속된 리소스 사용 시 연속성 검증
    {
        assert(b == a + 1 && "Descriptor slots must be contiguous");
    }
}

namespace ConstantBufferHelper
{
    class CBRing {
    public:
        CBRing(ID3D12Device* device, UINT ringCount, UINT bytesPerFrame);

        D3D12_GPU_VIRTUAL_ADDRESS PushBytes(UINT frameIndex, const void* src, UINT size);
        
        template<typename T>
        inline D3D12_GPU_VIRTUAL_ADDRESS Push(UINT frameIndex, const T& cb)
        {
            return PushBytes(frameIndex, &cb, (UINT)sizeof(T));
        }

        inline void BeginFrame(UINT frameIndex) { m_headPerFrame[frameIndex] = 0; }
        inline UINT Remaining(UINT frameIndex) const { return m_bytesPerFrame - m_headPerFrame[frameIndex]; }

    private:
        ComPtr<ID3D12Resource> m_buffer;
        uint8_t* m_cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS m_baseGPU = 0;
        std::vector<UINT> m_headPerFrame;
        UINT m_bytesPerFrame = 0;
        UINT m_ringCount = 0;
    };

    // CB 버퍼 바인딩
    template<typename T>
    void SetRootCBV(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, UINT rootParamIdx, CBRing& ring, UINT frameIdx, const T& cb)
    {
        auto gpuCB = ring.Push(frameIdx, cb);
        cmd->SetComputeRootConstantBufferView(rootParamIdx, gpuCB);
    }

    UINT CalcBytesPerFrame(const std::initializer_list<std::pair<UINT, UINT>> cbSizeAndCounts, float margin = 1.5f, UINT minFloor = 64*1024);

    // 용량 초과 시 임시 업로드 방식을 사용
    D3D12_GPU_VIRTUAL_ADDRESS PushOrSpill(ID3D12Device* device, CBRing& ring, UINT frameIdx, const void* src, UINT sizeBytes, std::vector<ComPtr<ID3D12Resource>>& pendingDeleteContainer);
}