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
    std::optional<std::string> rootsignature;
    std::optional<std::string> inputLayout;

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
    std::string rootSignature;
    std::string inputLayout;

    PSOShaderPaths shaders;
    PSORT          rt;
    PSORaster      raster;
    PSOBlend       blend;
    PSODepth       depth;
    std::string    topology = "triangle";
};

// --- InputElemnets ---
struct InputElementDesc
{
    std::string name;
    uint32_t index = 0;
    std::string format;
    uint32_t slot = 0;
    uint32_t byteOffset = 0;
    bool perInstance = false;
};

struct InputLayoutSpec
{
    std::string name;
    std::vector<InputElementDesc> descs;
};

struct InputLayoutSpecRaw {
    std::string name;
    std::optional<std::string> inherits;
    std::vector<InputElementDesc> descs;
};

// --- RootSignature ---
// Root Parameter 타입
enum class ERootParamType 
{ 
    Unknown,
    Constants, 
    CBV,
    SRV,
    UAV,
    Table 
};

// Descriptor Range
struct DescriptorRangeSpec 
{
    std::string name = "";
    ERootParamType type = ERootParamType::Table;
    uint32_t baseRegister = 0;
    uint32_t registerSpace = 0;
    int32_t count = 1;          // -1이면 Unbounded
    std::string flags = "None"; // "Volatile", "Static" ...
};

// Root Parameter
struct RootParamSpec 
{
    std::string name = "";
    ERootParamType type = ERootParamType::Table;
    uint32_t baseRegister = 0;
    uint32_t registerSpace = 0;
    uint32_t numConstants = 0; // Constants용
    std::vector<DescriptorRangeSpec> ranges; // Table용
    std::string flags = "None";
};

struct RootSignatureSpecRaw {
    std::string id;
    std::optional<std::string> inherits;
    std::vector<RootParamSpec> params;
};

struct RootSignatureSpec 
{
    std::string id;
    std::vector<RootParamSpec> params;
};

struct PipelineBundle 
{
    int schemaVersion = 1;
    std::vector<RootSignatureSpec> rsSpecs;
    std::vector<PSOSpec> psoSpecs;
    std::vector<InputLayoutSpec> iaSpecs;
};

PipelineBundle LoadPipelineBundle(LPCWSTR path);