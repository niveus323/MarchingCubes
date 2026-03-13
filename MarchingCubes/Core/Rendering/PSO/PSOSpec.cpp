#include "pch.h"
#include "PSOSpec.h"
#include "Core/Utils/FileUtils.h"
#include <stdexcept>
#include <algorithm>
#include <unordered_map>

// -------- Merge helpers (child overrides if has value) --------
static void merge(PSOShaderPaths& out, const PSOShaderPaths& parent, const PSOShaderPathsRaw& child) {
    out = parent;
    if (child.vs) out.vs = *child.vs;
    if (child.ps) out.ps = *child.ps;
    if (child.ds) out.ds = *child.ds;
    if (child.hs) out.hs = *child.hs;
    if (child.gs) out.gs = *child.gs;
}
static void merge(PSORT& out, const PSORT& parent, const PSORTRaw& child) {
    out = parent;
    if (child.format)    out.format = *child.format;
    if (child.dsv)       out.dsv = *child.dsv;
    if (child.msaa)      out.msaa = *child.msaa;
    if (child.depthOnly) out.depthOnly = *child.depthOnly;
}
static void merge(PSORaster& out, const PSORaster& parent, const PSORasterRaw& child) {
    out = parent;
    if (child.fill)     out.fill = *child.fill;
    if (child.cull)     out.cull = *child.cull;
    if (child.frontCCW) out.frontCCW = *child.frontCCW;
}
static void merge(PSOBlend& out, const PSOBlend& parent, const PSOBlendRaw& child) {
    out = parent;
    if (child.alpha) out.alpha = *child.alpha;
}
static void merge(PSODepth& out, const PSODepth& parent, const PSODepthRaw& child) {
    out = parent;
    if (child.enable) out.enable = *child.enable;
    if (child.write)  out.write = *child.write;
    if (child.func)   out.func = *child.func;
}

// -------- Parse a single raw item --------
static PSOSpecRaw parse_raw(const nlohmann::json& jp, int schema)
{
    PSOSpecRaw s; s.schemaVersion = schema;
    s.id = jp.at("id").get<std::string>();
    s.order = FileUtils::jopt_int(jp, "order");
    s.inherits = FileUtils::jopt_str(jp, "inherits");
    s.rootsignature = FileUtils::jopt_str(jp, "rootSignature");

    if (jp.contains("shaders")) 
    {
        const auto& js = jp["shaders"];
        s.shaders.vs = FileUtils::jopt_str(js, "vs");
        s.shaders.ps = FileUtils::jopt_str(js, "ps");
        s.shaders.ds = FileUtils::jopt_str(js, "ds");
        s.shaders.hs = FileUtils::jopt_str(js, "hs");
        s.shaders.gs = FileUtils::jopt_str(js, "gs");
    }
    
    if (jp.contains("renderTarget")) 
    {
        const auto& jr = jp["renderTarget"];
        s.rt.format = FileUtils::jopt_str(jr, "format");
        s.rt.dsv = FileUtils::jopt_str(jr, "dsv");
        s.rt.msaa = FileUtils::jopt_int(jr, "msaa");
        s.rt.depthOnly = FileUtils::jopt_bool(jr, "depthOnly");
    }
    
    if (jp.contains("raster")) 
    {
        const auto& jr = jp["raster"];
        s.raster.fill = FileUtils::jopt_str(jr, "fill");
        s.raster.cull = FileUtils::jopt_str(jr, "cull");
        s.raster.frontCCW = FileUtils::jopt_bool(jr, "frontCCW");
    }
    
    if (jp.contains("blend")) 
    {
        const auto& jb = jp["blend"];
        s.blend.alpha = FileUtils::jopt_bool(jb, "alpha");
    }

    if (jp.contains("depth")) 
    {
        const auto& jd = jp["depth"];
        s.depth.enable = FileUtils::jopt_bool(jd, "enable");
        s.depth.write = FileUtils::jopt_bool(jd, "write");
        s.depth.func = FileUtils::jopt_str(jd, "func");
    }
    s.topology = FileUtils::jopt_str(jp, "topology");
    return s;
}

// -------- Resolve inheritance (DFS with cycle detect) --------
enum class Visit { Not, Visiting, Done };

static PSOSpec ResolvePSOSpec(
    const std::string& id,
    const std::unordered_map<std::string, PSOSpecRaw>& raws,
    std::unordered_map<std::string, Visit>& state,
    std::unordered_map<std::string, PSOSpec>& memo)
{
    if (auto it = memo.find(id); it != memo.end()) 
        return it->second;

    auto itR = raws.find(id);
    if (itR == raws.end()) throw std::runtime_error("Unknown PSO id in inheritance: " + id);

    if (state[id] == Visit::Visiting) throw std::runtime_error("Cyclic inheritance detected at: " + id);
    if (state[id] == Visit::Done) return memo[id]; // shouldn't happen before memo

    state[id] = Visit::Visiting;
    const PSOSpecRaw& raw = itR->second;

    // parent (if any)
    PSOSpec parent; // defaults
    parent.schemaVersion = raw.schemaVersion;
    parent.id = id;
    parent.order = 0;
    parent.shaders = {};
    parent.rootSignature = {};
    parent.rt = {};
    parent.raster = {};
    parent.blend = {};
    parent.depth = {};
    parent.topology = "triangle";

    if (raw.inherits && !raw.inherits->empty()) 
    {
        parent = ResolvePSOSpec(*raw.inherits, raws, state, memo);
    }

    // merge child onto parent
    PSOSpec res = parent;
    res.schemaVersion = raw.schemaVersion;
    res.id = raw.id;
    if (raw.order) res.order = *raw.order;

    //기존에 쓰던 rootSignature가 명시되어 있지 않고 상속받을 rootSignature가 명시되어 있는 경우 이를 채용
    if (raw.rootsignature) 
        res.rootSignature = *raw.rootsignature; 

    merge(res.shaders, parent.shaders, raw.shaders);
    merge(res.rt, parent.rt, raw.rt);
    merge(res.raster, parent.raster, raw.raster);
    merge(res.blend, parent.blend, raw.blend);
    merge(res.depth, parent.depth, raw.depth);
    if (raw.topology) res.topology = *raw.topology;

    state[id] = Visit::Done;
    memo[id] = res;
    return res;
}

std::vector<PSOSpec> LoadPSOJsonResolved(_In_ nlohmann::json& root, int schema)
{
    // load raws
    std::unordered_map<std::string, PSOSpecRaw> raws;
    for (const auto& jp : root.at("pso")) 
    {
        PSOSpecRaw r = parse_raw(jp, schema);
        raws[r.id] = std::move(r);
    }

    // resolve
    std::unordered_map<std::string, Visit> state;
    std::unordered_map<std::string, PSOSpec> memo;

    for (auto& kv : raws) state[kv.first] = Visit::Not;
    for (auto& kv : raws) 
    {
        if (state[kv.first] == Visit::Done) continue; //중복 로드를 방지
        ResolvePSOSpec(kv.first, raws, state, memo);
    }

    std::vector<PSOSpec> out;
    out.reserve(raws.size());
    for (const auto& [id, spec] : memo)
    {
        out.push_back(spec);
    }

    // order sort
    std::sort(out.begin(), out.end(), [](const PSOSpec& a, const PSOSpec& b) {
        if (a.order != b.order) return a.order < b.order;
        return a.id < b.id;
    });

    return out;
}

static ERootParamType ParseRootParamType(const std::string& s) 
{
    if (s == "Constant") return ERootParamType::Constants;
    if (s == "CBV") return ERootParamType::CBV;
    if (s == "SRV") return ERootParamType::SRV;
    if (s == "UAV") return ERootParamType::UAV;
    if (s == "Table") return ERootParamType::Table;
    return ERootParamType::Unknown;
}

static RootParamSpec ParseRootParam(const nlohmann::json& j) 
{
    RootParamSpec spec;
    spec.type = ParseRootParamType(j.value("type", ""));
    spec.baseRegister = j.value("register", 0);
    spec.registerSpace = j.value("space", 0);
    spec.flags = j.value("flgas", 0);

    if (spec.type == ERootParamType::Constants) 
    {
        spec.numConstants = j.value("num32Bit", 0);
    }
    else if (spec.type == ERootParamType::Table) 
    {
        if (j.contains("ranges")) 
        {
            for (const auto& r : j["ranges"]) 
            {
                DescriptorRangeSpec range;
                range.type = ParseRootParamType(r.value("type", ""));
                range.baseRegister = r.value("register", 0);
                range.registerSpace = r.value("space", 0);
                range.count = r.value("count", 1);
                range.flags = r.value("flags", "None");
                spec.ranges.push_back(range);
            }
        }
    }
    return spec;
}

static RootSignatureSpecRaw ParseRootSigRaw(const nlohmann::json& j) 
{
    RootSignatureSpecRaw raw;
    raw.id = j.value("id", "Unknown");
    if (j.contains("inherits")) raw.inherits = j["inherits"];

    if (j.contains("params")) 
    {
        for (const auto& p : j["params"]) 
        {
            raw.params.push_back(ParseRootParam(p));
        }
    }
    return raw;
}

static RootSignatureSpec ResolveRSSpec(const std::string& id,
    const std::unordered_map<std::string, RootSignatureSpecRaw>& raws,
    std::unordered_map<std::string, RootSignatureSpec>& memo,
    std::vector<std::string>& callStack)
{
    if (memo.count(id)) return memo[id];

    // 순환 참조 방지
    for (const auto& s : callStack) 
    {
        if (s == id) throw std::runtime_error("Circular inheritance detected in RootSignatures: " + id);
    }
    callStack.push_back(id);

    auto it = raws.find(id);
    if (it == raws.end()) throw std::runtime_error("RootSignature ID not found: " + id);
    const auto& raw = it->second;

    RootSignatureSpec res;
    res.id = id;

    if (raw.inherits) 
    {
        RootSignatureSpec parent = ResolveRSSpec(*raw.inherits, raws, memo, callStack);
        res.params = parent.params;
    }
    res.params.insert(res.params.end(), raw.params.begin(), raw.params.end());

    memo[id] = res;
    callStack.pop_back();
    return res;
}

std::vector<RootSignatureSpec> LoadRSJsonResolved(_In_ nlohmann::json& root, int schema)
{
    // Load Raw RootSigs
    std::unordered_map<std::string, RootSignatureSpecRaw> rsRaws;
    if (root.contains("rootSignatures"))
    {
        for (const auto& item : root["rootSignatures"])
        {
            RootSignatureSpecRaw raw = ParseRootSigRaw(item);
            rsRaws[raw.id] = std::move(raw);
        }
    }

    // Resolve RootSigs
    std::vector<RootSignatureSpec> out;
    std::unordered_map<std::string, RootSignatureSpec> rsMemo;
    std::vector<std::string> rsStack;
    for (const auto& [id, raw] : rsRaws) out.push_back(ResolveRSSpec(id, rsRaws, rsMemo, rsStack));

    return out;
}

PipelineBundle LoadPipelineBundle(LPCWSTR path)
{
    nlohmann::json root;
    std::string pathStr = StringUtils::ToString(path);
    if (!FileUtils::ReadJSON(pathStr, root))
    {
        throw std::runtime_error("Cannot open or parse JSON file: " + pathStr);
    }

    int schemaVersion = root.value("schema", 1);
    return PipelineBundle{
        .schemaVersion = schemaVersion,
        .rsSpecs = LoadRSJsonResolved(root, schemaVersion),
        .psoSpecs = LoadPSOJsonResolved(root, schemaVersion)
    };
}