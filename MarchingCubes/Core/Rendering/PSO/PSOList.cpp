#include "pch.h"
#include "PSOList.h"
#include "PSOEnumLUT.h"
#include <filesystem>

static void ApplyAlphaBlend(D3D12_BLEND_DESC& b, uint32_t rtCount)
{
    for (uint32_t i = 0; i < rtCount && i < 8; i++) {
        auto& rt = b.RenderTarget[i];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
}

PSOList::PSOList(const BuildContext& ctx, const std::vector<PSOSpec>& specs)
{
    m_ids.clear();
    m_states.clear();
    m_idToIndex.clear();
    m_ids.reserve(specs.size());
    m_states.reserve(specs.size());

    for (const auto& s : specs) {
        ComPtr<ID3D12PipelineState> pso;

        switch (s.schemaVersion) {
            case 1:
            default:
                pso = CreatePSO_v1(ctx, s);
                break;
        }

        if (pso) {
            m_idToIndex[s.id] = (int)m_states.size();
            m_ids.push_back(s.id);
            m_states.push_back(std::move(pso));
        }
    }
}

ID3D12PipelineState* PSOList::Get(int index) const
{
    if (index < 0 || index >= (int)m_states.size()) return nullptr;
    return m_states[index].Get();
}

int PSOList::IndexOf(const std::string& id) const
{
    auto it = m_idToIndex.find(id);
    return (it == m_idToIndex.end()) ? -1 : it->second;
}

ComPtr<ID3D12PipelineState> PSOList::CreatePSO_v1(const BuildContext& ctx, const PSOSpec& s) const
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = ctx.root;
    d.InputLayout = ctx.inputLayout;

    // 셰이더
    ComPtr<ID3DBlob> VS = s.shaders.vs.empty() ? ComPtr<ID3DBlob>() : LoadFileBlob(s.shaders.vs);
    ComPtr<ID3DBlob> PS = s.shaders.ps.empty() ? ComPtr<ID3DBlob>() : LoadFileBlob(s.shaders.ps);
    ComPtr<ID3DBlob> DS = s.shaders.ds.empty() ? ComPtr<ID3DBlob>() : LoadFileBlob(s.shaders.ds);
    ComPtr<ID3DBlob> HS = s.shaders.hs.empty() ? ComPtr<ID3DBlob>() : LoadFileBlob(s.shaders.hs);
    ComPtr<ID3DBlob> GS = s.shaders.gs.empty() ? ComPtr<ID3DBlob>() : LoadFileBlob(s.shaders.gs);

    if (!s.shaders.vs.empty() && !VS) return nullptr; // 필수인 경우 실패
    if (!s.shaders.ps.empty() && !PS && s.topology != "line") return nullptr; // Filled/DebugNormal은 PS 기대

    if (VS) d.VS = { VS->GetBufferPointer(), VS->GetBufferSize() };
    if (PS) d.PS = { PS->GetBufferPointer(), PS->GetBufferSize() };
    if (DS) d.DS = { DS->GetBufferPointer(), DS->GetBufferSize() };
    if (HS) d.HS = { HS->GetBufferPointer(), HS->GetBufferSize() };
    if (GS) d.GS = { GS->GetBufferPointer(), GS->GetBufferSize() };

    // 기본값
    d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    d.SampleMask = UINT_MAX;

    // RT/DSV/MSAA
    d.NumRenderTargets = s.rt.depthOnly ? 0u : 1u;
    d.RTVFormats[0] = s.rt.depthOnly ? DXGI_FORMAT_UNKNOWN : ParseFormat(s.rt.format);
    d.DSVFormat = ParseFormat(s.rt.dsv);
    d.SampleDesc.Count = (uint32_t)std::max(1, s.rt.msaa);
    d.SampleDesc.Quality = 0;

    // Raster
    d.RasterizerState.FillMode = ParseFill(s.raster.fill);
    d.RasterizerState.CullMode = ParseCull(s.raster.cull);
    d.RasterizerState.FrontCounterClockwise = s.raster.frontCCW;

    // Blend
    if (s.blend.alpha && d.NumRenderTargets > 0)
        ApplyAlphaBlend(d.BlendState, d.NumRenderTargets);

    // Depth
    d.DepthStencilState.DepthEnable = s.depth.enable;
    d.DepthStencilState.DepthWriteMask = s.depth.write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = ParseCmp(s.depth.func);

    // Topology
    d.PrimitiveTopologyType = ParseTopo(s.topology);

    ComPtr<ID3D12PipelineState> pso;
    ThrowIfFailed(ctx.device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));

    return pso;
}

DXGI_FORMAT PSOList::ParseFormat(const std::string& s)
{
    return ParseEnum(s, kDXGIFormatLUT, DXGI_FORMAT_R8G8B8A8_UNORM);
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE PSOList::ParseTopo(const std::string& s)
{
    return ParseEnum(s, kTopoLUT, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
}

D3D12_FILL_MODE PSOList::ParseFill(const std::string& s)
{
    return ParseEnum(s, kFillLUT, D3D12_FILL_MODE_SOLID);
}

D3D12_CULL_MODE PSOList::ParseCull(const std::string& s)
{
    return ParseEnum(s, kCullLUT, D3D12_CULL_MODE_BACK);
}

D3D12_COMPARISON_FUNC PSOList::ParseCmp(const std::string& s)
{
    return ParseEnum(s, kCmpLUT, D3D12_COMPARISON_FUNC_LESS_EQUAL);
}

ComPtr<ID3DBlob> PSOList::LoadFileBlob(const std::string& path)
{
    if (path.empty()) return {};
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

    std::filesystem::path fullPath = exeDir / path;

    std::wstring wpath = fullPath.wstring();
    ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DReadFileToBlob(wpath.c_str(), &blob));
    return blob;
}