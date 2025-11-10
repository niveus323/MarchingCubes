#pragma once
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <d3d12.h>
#include <dxgiFormat.h>
#include <array>

// 입력 문자열을 정규화: 소문자, 공백/하이픈 제거, 언더스코어 유지
inline std::string Canonicalize(std::string_view s)
{
    std::string out; out.reserve(s.size());
    for (char c : s)
    {
        if (c == ' ' || c == '-') continue;
        out.push_back((char)std::tolower((unsigned char)c));
    }
    return out;
}

// 정렬된 LUT에서 탐색
template <typename Enum, size_t N>
constexpr Enum ParseEnum(std::string_view in, const std::array<std::pair<std::string_view, Enum>, N>& LUT, Enum default_value)
{
    const std::string key = Canonicalize(in);
    for (auto& kv : LUT)
    {
        if (kv.first == key)
            return kv.second;
    }

    return default_value;
}

// DXGI_FORMAT
// 키는 canonical string: "r8g8b8a8_unorm" → "r8g8b8a8_unorm" (언더스코어 유지)
static constexpr auto kDXGIFormatLUT = std::to_array<std::pair<std::string_view, DXGI_FORMAT>>({
    std::pair{"r8g8b8a8_unorm",   DXGI_FORMAT_R8G8B8A8_UNORM},
    std::pair{"r11g11b10_float",  DXGI_FORMAT_R11G11B10_FLOAT},
    std::pair{"d24_unorm_s8_uint",DXGI_FORMAT_D24_UNORM_S8_UINT},
    std::pair{"d32_float",        DXGI_FORMAT_D32_FLOAT},
    std::pair{"unknown",          DXGI_FORMAT_UNKNOWN},
    });

// Topology
static constexpr auto kTopoLUT = std::to_array<std::pair<std::string_view, D3D12_PRIMITIVE_TOPOLOGY_TYPE>>({
    std::pair{"triangle", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE},
    std::pair{"line",     D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE},
    std::pair{"point",    D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT},
    });

// Raster Fill
static constexpr auto kFillLUT = std::to_array<std::pair<std::string_view, D3D12_FILL_MODE>>({
    std::pair{"solid",     D3D12_FILL_MODE_SOLID},
    std::pair{"wireframe", D3D12_FILL_MODE_WIREFRAME},
    });

// Cull
static constexpr auto kCullLUT = std::to_array<std::pair<std::string_view, D3D12_CULL_MODE>>({
    std::pair{"none",  D3D12_CULL_MODE_NONE},
    std::pair{"front", D3D12_CULL_MODE_FRONT},
    std::pair{"back",  D3D12_CULL_MODE_BACK},
    });

// Depth func (별칭까지 포함)
static constexpr auto kCmpLUT = std::to_array<std::pair<std::string_view, D3D12_COMPARISON_FUNC>>({
    std::pair{"less",          D3D12_COMPARISON_FUNC_LESS},
    std::pair{"equal",         D3D12_COMPARISON_FUNC_EQUAL},
    std::pair{"lessequal",     D3D12_COMPARISON_FUNC_LESS_EQUAL},
    std::pair{"less_equal",    D3D12_COMPARISON_FUNC_LESS_EQUAL},
    std::pair{"greater",       D3D12_COMPARISON_FUNC_GREATER},
    std::pair{"greaterequal",  D3D12_COMPARISON_FUNC_GREATER_EQUAL},
    std::pair{"greater_equal", D3D12_COMPARISON_FUNC_GREATER_EQUAL},
    std::pair{"always",        D3D12_COMPARISON_FUNC_ALWAYS},
    });