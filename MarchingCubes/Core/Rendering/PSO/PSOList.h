#pragma once
#include "Core/Rendering/PSO/PSOSpec.h"
#include <unordered_map>

class PSOList {
public:
    struct BuildContext {
        ID3D12Device* device = nullptr;
        D3D12_INPUT_LAYOUT_DESC inputLayout{};
    };

    struct PipelineEntry
    {
        ID3D12PipelineState* pso = nullptr;
        ID3D12RootSignature* rs = nullptr;
    };

    PSOList(const BuildContext& ctx, const std::vector<PSOSpec>& specs, const std::vector<RootSignatureSpec>& rsSpecs);

    int GetCounts() const { return (int)m_pipelineStates.size(); }
    PipelineEntry Get(int index) const;
    PipelineEntry Get(std::string_view id) const;
    int  IndexOf(std::string_view id) const;
    int  GetRSIndex(uint16_t psoIndex) const { return m_psoToRSIndex[psoIndex]; }
    
    static ComPtr<ID3DBlob> LoadFileBlob(const std::string& path);
private:
    void CreateRootSignature(ID3D12Device* device, const std::vector<RootSignatureSpec>& specs);
    bool CreatePSODesc_v1(_In_ const PSOSpec& s, _Inout_ D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, _Inout_ std::vector<ComPtr<ID3DBlob>>& blobs) const;

    static DXGI_FORMAT ParseFormat(const std::string& s);
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE ParseTopology(const std::string& s);
    static D3D12_FILL_MODE  ParseFillMode(const std::string& s);
    static D3D12_CULL_MODE  ParseCullMode(const std::string& s);
    static D3D12_COMPARISON_FUNC ParseCmpFunc(const std::string& s);

    static std::string GetRootParamInfo(int index, const RootParamSpec& spec);
private:
    std::unordered_map<std::string, uint16_t> m_psoLookUp;
    std::unordered_map<std::string, uint16_t> m_rsLookUp;
    std::vector<uint16_t> m_psoToRSIndex;
    std::vector<ComPtr<ID3D12RootSignature>> m_rootSignatures;
    std::vector<ComPtr<ID3D12PipelineState>> m_pipelineStates;
};