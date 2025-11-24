#pragma once
#include "Core/Rendering/PSO/PSOSpec.h"
#include <unordered_map>

class PSOList {
public:
    struct BuildContext {
        ID3D12Device* device = nullptr;
        ID3D12RootSignature* root = nullptr;
        D3D12_INPUT_LAYOUT_DESC inputLayout{};
    };

    PSOList(const BuildContext& ctx, const std::vector<PSOSpec>& specs);

    int  Count() const { return (int)m_states.size(); }
    ID3D12PipelineState* Get(int index) const;
    int  IndexOf(const std::string& id) const;
    const std::string& IdAt(int index) const { return m_ids[index]; }

    static ComPtr<ID3DBlob> LoadFileBlob(const std::string& path);
private:
    ComPtr<ID3D12PipelineState> CreatePSO_v1(const BuildContext& ctx, const PSOSpec& s) const;

    static DXGI_FORMAT ParseFormat(const std::string& s);
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE ParseTopo(const std::string& s);
    static D3D12_FILL_MODE  ParseFill(const std::string& s);
    static D3D12_CULL_MODE  ParseCull(const std::string& s);
    static D3D12_COMPARISON_FUNC ParseCmp(const std::string& s);


private:
    std::vector<std::string> m_ids;
    std::vector<ComPtr<ID3D12PipelineState>> m_states;
    std::unordered_map<std::string, int> m_idToIndex;
};