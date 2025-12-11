#pragma once
#include <stdexcept>
#include <DirectXMath.h>
#include <span>
#include <filesystem>
#include <cwctype>

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

inline std::wstring ToLowerCopy(const std::wstring& s)
{
    std::wstring r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return r;
}

namespace MathHelper
{
    static inline uint32_t SafeSub(uint32_t a, uint32_t b)
    {
        return (a <= b) ? 0u : (a - b);
    }
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

inline void ThrowIfFalse(bool result, std::string_view msg)
{
    if (!result)
        throw std::runtime_error(msg.data());
}

enum class AssetType 
{ 
    Default = 0,
    Texture, 
    Shader, 
    Audio, 
    Data, 
};

inline std::filesystem::path GetFullPath(AssetType type, LPCWSTR name)
{
    const wchar_t* root = nullptr;

    switch (type)
    {
        case AssetType::Default: root = ASSETS_ROOT; break;
        case AssetType::Shader:  root = SHADERS_ROOT; break;
        case AssetType::Texture: root = TEXTURES_ROOT; break;
        default:                 root = ASSETS_ROOT; break;
    }

    std::filesystem::path p{ root };  
    p /= name;                        
    return p;
}
std::wstring GetShaderFullPath(LPCWSTR shaderName);

HRESULT ReadDataFromFile(LPCWSTR filename, byte** data, uint32_t* size);

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
