#pragma once
#include "Core/Rendering/PSO/PSOSpec.h"

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

    int GetCounts() const { return (int)m_psos.size(); }
    PipelineEntry Get(int index) const;
    PipelineEntry Get(std::string_view id) const;
    int  IndexOf(std::string_view id) const;
    int  GetRSIndex(uint16_t psoIndex) const { return m_psos[psoIndex].rootSignatureIndex; }
    uint32_t GetRootParameterIndex(uint16_t rsIndex, std::string_view paramName) { return m_rootSignatures[rsIndex].GetParameterIndex(paramName); }
    
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
    struct RootSignatureMeta
    {
        ComPtr<ID3D12RootSignature> rootSignature;
        std::string name;

        struct ParamMap
        {
            std::string key;
            uint32_t rootParameterIndex;
        };
        std::vector<ParamMap> parameterMap;

        struct DescriptorMap
        {
            std::string key;
            uint32_t offset = 0;
        };
        std::vector<DescriptorMap> descriptorMap;

        uint32_t GetParameterIndex(std::string_view key) const
        {
            for (const auto& param : parameterMap)
            {
                if (param.key == key) return param.rootParameterIndex;
            }
            return UINT32_MAX;
        }

        uint32_t GetDescriptorOffset(std::string_view key) const
        {
            for (const auto& descriptor : descriptorMap)
            {
                if (descriptor.key == key) return descriptor.offset;
            }
            return UINT32_MAX;
        }
    };
    struct PipelineStateMeta
    {
        ComPtr<ID3D12PipelineState> pso;
        uint16_t rootSignatureIndex = 0;
        std::string name = "Default";
    };
    std::vector<RootSignatureMeta> m_rootSignatures;
    std::vector<PipelineStateMeta> m_psos;
};