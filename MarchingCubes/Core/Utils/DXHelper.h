#pragma once
#include <stdexcept>
#include <DirectXMath.h>
#include <span>

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;
using namespace DirectX;

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<uint32_t>(hr));
	return std::string(s_str);
}

inline std::string UTF16ToUTF8(const wchar_t* wstr)
{
    if (!wstr || *wstr == L'\0') return {};

    // null 포함한 필요 길이
    int sizeNeeded = ::WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);

    // 빈 문자열이거나 오류
    if (sizeNeeded <= 1) return {};

    std::string result(static_cast<size_t>(sizeNeeded - 1), '\0'); // null 제외한 길이
    ::WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), sizeNeeded, nullptr, nullptr);

    return result;
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
    case AssetType::Default: root = ASSETS_ROOT; break;
    case AssetType::Shader:  root = SHADERS_ROOT; break;
    case AssetType::Texture: root = TEXTURES_ROOT; break;
    default:                  root = ASSETS_ROOT; break;
    }
    return std::wstring(root) + L"/" + std::wstring(name);
}

std::wstring GetShaderFullPath(LPCWSTR shaderName);

HRESULT ReadDataFromFile(LPCWSTR filename, byte** data, uint32_t* size);

HRESULT ReadDataFromDDSFile(LPCWSTR filename, byte** data, uint32_t* offset, uint32_t* size);

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, uint32_t index)
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
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, uint32_t)
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

inline uint32_t CalculateConstantBufferByteSize(uint32_t byteSize)
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
    uint32_t compileFlags = 0;
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

static uint32_t AlignUp(uint32_t size, uint32_t align)
{
    return (size + align - 1) & ~(align - 1);
}

static uint64_t AlignUp64(uint64_t size, uint64_t align)
{
    return (size + align - 1) & ~(align - 1);
}

namespace MCUtil {
    inline uint32_t AlignCBSize(uint32_t size)
    {
        return AlignUp(size, 256u);
    }

    // 업로드 힙 상수버퍼 (즉시 Map/memcpy) 생성 함수
    void CreateUploadConstantBuffer(ID3D12Device* device, const void* data, size_t sizeBytes, ComPtr<ID3D12Resource>& outUpload);

    // Defaul 버퍼 + 업로드 copy (data가 nullptr이면 빈 버퍼만 생성)
    void CreateAndUploadStructuredBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* data, uint32_t numElements, uint32_t stride, ComPtr<ID3D12Resource>& outDefault, ComPtr<ID3D12Resource>* outUpload);

    // AppendStructuredUAV 버퍼 + Counter 버퍼 생성
    void CreateStructuredUavWithCounter(ID3D12Device* device, uint32_t numElements, uint32_t stride, ComPtr<ID3D12Resource>& outBuffer, ComPtr<ID3D12Resource>& outCounter);

    // 카운터 값을 Readback 버퍼로 복사 (4-Bytes)
    void CopyCounterToReadback(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* counter, ID3D12Resource* readback4B);

    // 버퍼 내용을 Readback 버퍼로 복사
    void CopyBufferToReadback(ID3D12GraphicsCommandList* cmd, ID3D12Resource* src, ID3D12Resource* readback, size_t bytes);

    // 3D Density Field 유틸리티

    // 3D Density Field 초기화 (SRV/UAV 디스크립터는 별도 생성)
    void CreateOrUpdateDensity3D(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, uint32_t dimX, uint32_t dimY, uint32_t dimZ, const float* srcLinearXYZ, ComPtr<ID3D12Resource>& ioTex3D, ComPtr<ID3D12Resource>* outUpload);

    static void EnsureZeroUpload(ID3D12Device* device);

    // Append Counter 0으로 리셋
    void ResetAndTransitCounter(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, ID3D12Resource* counter, D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

namespace DescriptorHelper
{
    void CreateSRV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU);
    void CreateUAV_Texture3D(ID3D12Device* device, ID3D12Resource* res, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter = nullptr);
    void CreateSRV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU);
    void CreateUAV_Structured(ID3D12Device* device, ID3D12Resource* res, uint32_t stride, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, ID3D12Resource* counter = nullptr);
    void CreateUAV_Raw(ID3D12Device* device, ID3D12Resource* res, D3D12_CPU_DESCRIPTOR_HANDLE dstCPU, uint32_t firstElement = 0, uint32_t numElements = 0);
}

namespace ConstantBufferHelper
{
    class CBRing {
    public:
        CBRing(ID3D12Device* device, uint32_t ringCount, uint32_t bytesPerFrame);

        D3D12_GPU_VIRTUAL_ADDRESS PushBytes(uint32_t frameIndex, const void* src, uint32_t size);
        
        template<typename T>
        inline D3D12_GPU_VIRTUAL_ADDRESS Push(uint32_t frameIndex, const T& cb)
        {
            return PushBytes(frameIndex, &cb, (uint32_t)sizeof(T));
        }

        inline void BeginFrame(uint32_t frameIndex) { m_headPerFrame[frameIndex] = 0; }
        inline uint32_t Remaining(uint32_t frameIndex) const { return m_bytesPerFrame - m_headPerFrame[frameIndex]; }

    private:
        ComPtr<ID3D12Resource> m_buffer;
        uint8_t* m_cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS m_baseGPU = 0;
        std::vector<uint32_t> m_headPerFrame;
        uint32_t m_bytesPerFrame = 0;
        uint32_t m_ringCount = 0;
    };

    // CB 버퍼 바인딩
    template<typename T>
    void SetRootCBV(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, uint32_t rootParamIdx, CBRing& ring, uint32_t frameIdx, const T& cb)
    {
        auto gpuCB = ring.Push(frameIdx, cb);
        cmd->SetComputeRootConstantBufferView(rootParamIdx, gpuCB);
    }

    uint32_t CalcBytesPerFrame(const std::initializer_list<std::pair<uint32_t, uint32_t>> cbSizeAndCounts, float margin = 1.5f, uint32_t minFloor = 64*1024);

    // 용량 초과 시 임시 업로드 방식을 사용
    D3D12_GPU_VIRTUAL_ADDRESS PushOrSpill(ID3D12Device* device, CBRing& ring, uint32_t frameIdx, const void* src, uint32_t sizeBytes, std::vector<ComPtr<ID3D12Resource>>& pendingDeleteContainer);
}