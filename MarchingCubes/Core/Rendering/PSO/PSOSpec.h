#pragma once
#include <optional>

// ------------ Raw (optional) structs: JSON에서 읽은 "원본" ------------
struct PSOShaderPathsRaw { std::optional<std::string> vs, ps, ds, hs, gs; };
struct PSORTRaw { std::optional<std::string> format, dsv; std::optional<int> msaa; std::optional<bool> depthOnly; };
struct PSORasterRaw { std::optional<std::string> fill, cull; std::optional<bool> frontCCW; };
struct PSOBlendRaw { std::optional<bool> alpha; };
struct PSODepthRaw { std::optional<bool> enable, write; std::optional<std::string> func; };

struct PSOSpecRaw {
    int schemaVersion = 1;
    std::string id;
    std::optional<int> order;
    std::optional<std::string> inherits;

    PSOShaderPathsRaw shaders;
    PSORTRaw          rt;
    PSORasterRaw      raster;
    PSOBlendRaw       blend;
    PSODepthRaw       depth;
    std::optional<std::string> topology; // "triangle" | "line" | "point"
};

// ------------ Resolved (concrete) structs: 상속/기본값 적용 완료 ------------
struct PSOShaderPaths { std::string vs, ps, ds, hs, gs; };
struct PSORT { std::string format = "R8G8B8A8_UNORM", dsv = "D32_FLOAT"; int msaa = 1; bool depthOnly = false; };
struct PSORaster { std::string fill = "solid", cull = "back"; bool frontCCW = false; };
struct PSOBlend { bool alpha = false; };
struct PSODepth { bool enable = true, write = true; std::string func = "less_equal"; };

struct PSOSpec {
    int         schemaVersion = 1;
    std::string id;
    int         order = 0;
    PSOShaderPaths shaders;
    PSORT          rt;
    PSORaster      raster;
    PSOBlend       blend;
    PSODepth       depth;
    std::string    topology = "triangle";
};

// JSON 로드 + 상속 해석까지 완료된 리스트를 반환
std::vector<PSOSpec> LoadPSOJsonResolved(LPCWSTR path, int* outSchema = nullptr);