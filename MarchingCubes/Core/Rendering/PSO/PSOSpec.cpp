#include "pch.h"
#include "PSOSpec.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// -------- JSON helpers --------
static std::optional<std::string> jopt_str(const json& j, const char* k) {
    auto it = j.find(k); if (it != j.end() && it->is_string()) return it->get<std::string>(); return std::nullopt;
}
static std::optional<bool> jopt_bool(const json& j, const char* k) {
    auto it = j.find(k); if (it != j.end() && it->is_boolean()) return it->get<bool>(); return std::nullopt;
}
static std::optional<int> jopt_int(const json& j, const char* k) {
    auto it = j.find(k); if (it != j.end() && it->is_number_integer()) return it->get<int>(); return std::nullopt;
}

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
static PSOSpecRaw parse_raw(const json& jp, int schema)
{
    PSOSpecRaw s; s.schemaVersion = schema;
    s.id = jp.at("id").get<std::string>();
    s.order = jopt_int(jp, "order");
    s.inherits = jopt_str(jp, "inherits");

    if (jp.contains("shaders")) {
        const auto& js = jp["shaders"];
        s.shaders.vs = jopt_str(js, "vs");
        s.shaders.ps = jopt_str(js, "ps");
        s.shaders.ds = jopt_str(js, "ds");
        s.shaders.hs = jopt_str(js, "hs");
        s.shaders.gs = jopt_str(js, "gs");
    }
    if (jp.contains("rt")) {
        const auto& jr = jp["rt"];
        s.rt.format = jopt_str(jr, "format");
        s.rt.dsv = jopt_str(jr, "dsv");
        s.rt.msaa = jopt_int(jr, "msaa");
        s.rt.depthOnly = jopt_bool(jr, "depthOnly");
    }
    if (jp.contains("raster")) {
        const auto& jr = jp["raster"];
        s.raster.fill = jopt_str(jr, "fill");
        s.raster.cull = jopt_str(jr, "cull");
        s.raster.frontCCW = jopt_bool(jr, "frontCCW");
    }
    if (jp.contains("blend")) {
        const auto& jb = jp["blend"];
        s.blend.alpha = jopt_bool(jb, "alpha");
    }
    if (jp.contains("depth")) {
        const auto& jd = jp["depth"];
        s.depth.enable = jopt_bool(jd, "enable");
        s.depth.write = jopt_bool(jd, "write");
        s.depth.func = jopt_str(jd, "func");
    }
    s.topology = jopt_str(jp, "topology");
    return s;
}

// -------- Resolve inheritance (DFS with cycle detect) --------
enum class Visit { Not, Visiting, Done };

static PSOSpec resolve_one(
    const std::string& id,
    const std::unordered_map<std::string, PSOSpecRaw>& raws,
    std::unordered_map<std::string, Visit>& state,
    std::unordered_map<std::string, PSOSpec>& memo)
{
    if (auto it = memo.find(id); it != memo.end()) return it->second;

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
    parent.rt = {};
    parent.raster = {};
    parent.blend = {};
    parent.depth = {};
    parent.topology = "triangle";

    if (raw.inherits && !raw.inherits->empty()) {
        parent = resolve_one(*raw.inherits, raws, state, memo);
    }

    // merge child onto parent
    PSOSpec res = parent;
    res.schemaVersion = raw.schemaVersion;
    res.id = raw.id;
    if (raw.order) res.order = *raw.order;

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

std::vector<PSOSpec> LoadPSOJsonResolved(LPCWSTR path, int* outSchema)
{
    FILE* fp = nullptr;
    errno_t err = _wfopen_s(&fp, path, L"rb");
    if (err != 0 || fp == nullptr)
    {
        std::wstring msg = L"Cannot open: " + std::wstring(path);
        throw std::runtime_error(std::string(msg.begin(), msg.end()));
    }
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        throw std::runtime_error("fseek failed");
    }
    long size = ftell(fp);
    if (size < 0)
    {
        fclose(fp);
        throw std::runtime_error("ftell failed");
    }

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        throw std::runtime_error("fseek rewind failed");
    }

    std::string buffer;
    buffer.resize(static_cast<size_t>(size));

    if (size > 0)
    {
        size_t readCount = fread(buffer.data(), 1, buffer.size(), fp);
        if (readCount != buffer.size())
        {
            fclose(fp);
            throw std::runtime_error("fread failed (partial read)");
        }
    }

    fclose(fp);

    json root = json::parse(buffer);

    int schema = root.value("schema", 1);
    if (outSchema) *outSchema = schema;

    // load raws
    std::unordered_map<std::string, PSOSpecRaw> raws;
    for (const auto& jp : root.at("pso")) {
        PSOSpecRaw r = parse_raw(jp, schema);
        raws[r.id] = std::move(r);
    }

    // resolve all
    std::vector<PSOSpec> out;
    out.reserve(raws.size());
    std::unordered_map<std::string, Visit> state;
    std::unordered_map<std::string, PSOSpec> memo;

    for (auto& kv : raws) state[kv.first] = Visit::Not;
    for (auto& kv : raws) {
        if (state[kv.first] == Visit::Done) continue;
        out.push_back(resolve_one(kv.first, raws, state, memo));
    }

    // order sort
    std::sort(out.begin(), out.end(), [](const PSOSpec& a, const PSOSpec& b) {
        if (a.order != b.order) return a.order < b.order;
        return a.id < b.id;
        });
    return out;
}