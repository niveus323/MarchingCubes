#include "pch.h"
#include "PSOList.h"
#include "PSOEnumLUT.h"
#include <filesystem>
#include "Core/Utils/StringUtils.h"
#include <list>
#include <cstdlib>

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

static D3D12_ROOT_DESCRIPTOR_FLAGS ParseDescripotrFlags(const std::string& flags)
{
	if (flags == "Volatile") return D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
	if (flags == "Static") return D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	if (flags == "StaticWhileSetAtExecute") return D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
	return D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
}

static D3D12_DESCRIPTOR_RANGE_FLAGS ParseRangeFlags(const std::string& flags) 
{
	if (flags == "Volatile") return D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
	if (flags == "DataVolatile") return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	if (flags == "DataStatic") return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	if (flags == "StaticWhileSetAtExecute") return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
	return D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
}

PSOList::PSOList(ID3D12Device* device, const std::vector<PSOSpec>& specs, const std::vector<RootSignatureSpec>& rsSpecs, const std::vector<InputLayoutSpec>& iaSpecs)
{
	//InputElement
	CreateInputElements(device, iaSpecs);

	// RootSignature 
	CreateRootSignature(device, rsSpecs);

	// PSO
	m_psos.clear();
	m_psos.reserve(specs.size());
	
	for (const auto& spec : specs)
	{
		std::vector<ComPtr<ID3DBlob>> aliveBlobs;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		PipelineStateMeta psoMeta{ .name = spec.id };

		for (UINT i = 0; i < m_rootSignatures.size(); ++i)
		{
			if (m_inputLayouts.contains(spec.inputLayout))
			{
				psoDesc.InputLayout = m_inputLayouts[spec.inputLayout];
			}

			if (m_rootSignatures[i].name == spec.rootSignature)
			{
				psoMeta.rootSignatureIndex = static_cast<uint16_t>(i);
				psoDesc.pRootSignature = m_rootSignatures[i].rootSignature.Get();
			}
		}

		bool result = false;
		switch (spec.schemaVersion) {
			case 1:
			default:
			{
				result = CreatePSODesc_v1(spec, psoDesc, aliveBlobs);
			}
			break;
		}

		if (result)
		{
			ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(psoMeta.pso.ReleaseAndGetAddressOf())));
			m_psos.push_back(std::move(psoMeta));
		}
	}
}

PSOList::PipelineEntry PSOList::Get(int index) const
{
	return PSOList::PipelineEntry{
		.pso = m_psos[index].pso.Get(),
		.rs = m_rootSignatures[m_psos[index].rootSignatureIndex].rootSignature.Get()
	};
}

PSOList::PipelineEntry PSOList::Get(std::string_view id) const
{
	for (int i = 0; i < m_psos.size(); ++i)
	{
		if (m_psos[i].name == id)
		{
			return Get(i);
		}
	}
	return PSOList::PipelineEntry{};
}

int PSOList::IndexOf(std::string_view id) const
{
	for (int i = 0; i < m_psos.size(); ++i)
	{
		if (m_psos[i].name == id)
		{
			return i;
		}
	}
	return -1;
}

void PSOList::CreateInputElements(ID3D12Device* device, const std::vector<InputLayoutSpec>& specs)
{
	bool bParseFailed = false;
	for (const auto& spec : specs)
	{
		auto& descs = m_inputElementDescs[spec.name];
#ifdef _DEBUG
		std::string debugLog = "\n=== InputElements Layout: " + spec.name + " ===\n";
#endif // _DEBUG
		for (const auto& desc : spec.descs)
		{
			m_inputElementNames.push_back(desc.name);
			D3D12_INPUT_ELEMENT_DESC parsedDesc{
				.SemanticName = desc.name.c_str(),
				.SemanticIndex = desc.index,
				.Format = ParseFormat(desc.format),
				.AlignedByteOffset = desc.byteOffset,
				.InputSlotClass = desc.perInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = desc.perInstance ? 1u : 0u
			};
			descs.push_back(parsedDesc);
		}
		m_inputLayouts[spec.name] = D3D12_INPUT_LAYOUT_DESC{
			.pInputElementDescs = descs.data(),
			.NumElements = static_cast<UINT>(descs.size())
		};
	}
}

void PSOList::CreateRootSignature(ID3D12Device* device, const std::vector<RootSignatureSpec>& specs)
{
	bool bParseFailed = false;
	for (const auto& spec : specs)
	{
		std::vector<CD3DX12_ROOT_PARAMETER1> rootParams;
		std::list<std::vector<CD3DX12_DESCRIPTOR_RANGE1>> rangesStore;
		RootSignatureMeta rsMeta{
			.name = spec.id
		};
#ifdef _DEBUG
		std::string debugLog = "\n=== RootSignature Layout: " + spec.id + " ===\n";
#endif // _DEBUG

		for (size_t i = 0; i < spec.params.size(); ++i)
		{
			const auto& rootParam = spec.params[i];
			rsMeta.parameterMap.push_back(RootSignatureMeta::ParamMap{
				.key = rootParam.name,
				.rootParameterIndex = static_cast<uint32_t>(i)
			});
#ifdef _DEBUG
			debugLog += GetRootParamInfo((int)i, rootParam) + "\n";
#endif // _DEBUG

			CD3DX12_ROOT_PARAMETER1 param{};
			switch (rootParam.type)
			{
				case ERootParamType::Constants:
				{
					param.InitAsConstants(rootParam.numConstants, rootParam.baseRegister, rootParam.registerSpace);
				}
				break;
				case ERootParamType::CBV:
				{
					param.InitAsConstantBufferView(rootParam.baseRegister, rootParam.registerSpace, ParseDescripotrFlags(rootParam.flags));
				}
				break;
				case ERootParamType::SRV:
				{
					param.InitAsShaderResourceView(rootParam.baseRegister, rootParam.registerSpace, ParseDescripotrFlags(rootParam.flags));
				}
				break;
				case ERootParamType::UAV:
				{
					param.InitAsUnorderedAccessView(rootParam.baseRegister, rootParam.registerSpace, ParseDescripotrFlags(rootParam.flags));
				}
				break;
				case ERootParamType::Table:
				{
					auto& d3dRanges = rangesStore.emplace_back();
					uint32_t countSum = 0;
					for (auto iter = rootParam.ranges.begin(); iter != rootParam.ranges.end(); ++iter)
					{
						uint32_t count = 0;
						if (iter->count == -1)
						{
							if ((iter + 1) != rootParam.ranges.end())
							{
								Log::Print(ELogVerbosity::Fatal, "PSO", "Unbounded Range Should be Last!!!");
								bParseFailed = true;
								break;
							}
							count = UINT32_MAX;
						}
						else
						{
							count = static_cast<uint32_t>(iter->count);
						}

						D3D12_DESCRIPTOR_RANGE_TYPE type;
						if (iter->type == ERootParamType::CBV) type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
						else if (iter->type == ERootParamType::UAV) type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
						else if (iter->type == ERootParamType::SRV) type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
						else continue;

						d3dRanges.push_back(CD3DX12_DESCRIPTOR_RANGE1(
							type,
							count,
							iter->baseRegister,
							iter->registerSpace,
							ParseRangeFlags(iter->flags)
						));

						rsMeta.descriptorMap.push_back(RootSignatureMeta::DescriptorMap{
							.key = iter->name,
							.offset = countSum
						});

						if (count != UINT32_MAX) countSum += count;
					}
					if (bParseFailed) break;
					param.InitAsDescriptorTable((UINT)d3dRanges.size(), d3dRanges.data());
				}
				break;
				default:
				{
					// 잘못된 RootParameter 타입. 초기화하지 않고 넘어간다
				}
				break;
			}

			rootParams.push_back(param);
		}

		if (bParseFailed) return;

#ifdef _DEBUG
		OutputDebugStringA(debugLog.c_str());
#endif
		// Static Sampler 등록 ( 런타임에 바꿔야할 샘플러가 필요할 경우 Descriptor Table에 포함할 것.)
		CD3DX12_STATIC_SAMPLER_DESC samplerDescs = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR); // s0
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(static_cast<UINT>(rootParams.size()), rootParams.data(), 1, &samplerDescs, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signatureBlob;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signatureBlob, nullptr));

		ThrowIfFailed(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rsMeta.rootSignature.ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT_ALIAS(rsMeta.rootSignature, std::wstring(spec.id.begin(), spec.id.end()).c_str());

		m_rootSignatures.push_back(std::move(rsMeta));
	}
}

bool PSOList::CreatePSODesc_v1(_In_ const PSOSpec& s, _Inout_ D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, _Inout_ std::vector<ComPtr<ID3DBlob>>& blobs) const
{
	auto LoadShader = [&blobs](std::string path) -> ComPtr<ID3DBlob> {
		if (path.empty()) return ComPtr<ID3D10Blob>();

		auto blob = LoadFileBlob(path);
		blobs.push_back(blob);
		return blob;
	};

	// 셰이더
	ComPtr<ID3DBlob> VS = LoadShader(s.shaders.vs);
	ComPtr<ID3DBlob> PS = LoadShader(s.shaders.ps);
	ComPtr<ID3DBlob> DS = LoadShader(s.shaders.ds);
	ComPtr<ID3DBlob> HS = LoadShader(s.shaders.hs);
	ComPtr<ID3DBlob> GS = LoadShader(s.shaders.gs);

	if (!s.shaders.vs.empty() && !VS) return false; // 필수인 경우 실패
	if (!s.shaders.ps.empty() && !PS && s.topology != "line") return false; // Filled/DebugNormal은 PS 기대

	if (VS) desc.VS = { VS->GetBufferPointer(), VS->GetBufferSize() };
	if (PS) desc.PS = { PS->GetBufferPointer(), PS->GetBufferSize() };
	if (DS) desc.DS = { DS->GetBufferPointer(), DS->GetBufferSize() };
	if (HS) desc.HS = { HS->GetBufferPointer(), HS->GetBufferSize() };
	if (GS) desc.GS = { GS->GetBufferPointer(), GS->GetBufferSize() };

	// 기본값
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;

	// RT/DSV/MSAA
	desc.NumRenderTargets = s.rt.depthOnly ? 0u : 1u;
	desc.RTVFormats[0] = s.rt.depthOnly ? DXGI_FORMAT_UNKNOWN : ParseFormat(s.rt.format);
	desc.DSVFormat = ParseFormat(s.rt.dsv);
	desc.SampleDesc.Count = (uint32_t)std::max(1, s.rt.msaa);
	desc.SampleDesc.Quality = 0;

	// Raster
	desc.RasterizerState.FillMode = ParseFillMode(s.raster.fill);
	desc.RasterizerState.CullMode = ParseCullMode(s.raster.cull);
	desc.RasterizerState.FrontCounterClockwise = s.raster.frontCCW;

	// Blend
	if (s.blend.alpha && desc.NumRenderTargets > 0) ApplyAlphaBlend(desc.BlendState, desc.NumRenderTargets);

	// Depth
	desc.DepthStencilState.DepthEnable = s.depth.enable;
	desc.DepthStencilState.DepthWriteMask = s.depth.write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = ParseCmpFunc(s.depth.func);

	// Topology
	desc.PrimitiveTopologyType = ParseTopology(s.topology);

	return true;
}

DXGI_FORMAT PSOList::ParseFormat(const std::string& s)
{
	return ParseEnum(s, kDXGIFormatLUT, DXGI_FORMAT_R8G8B8A8_UNORM);
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE PSOList::ParseTopology(const std::string& s)
{
	return ParseEnum(s, kTopoLUT, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
}

D3D12_FILL_MODE PSOList::ParseFillMode(const std::string& s)
{
	return ParseEnum(s, kFillLUT, D3D12_FILL_MODE_SOLID);
}

D3D12_CULL_MODE PSOList::ParseCullMode(const std::string& s)
{
	return ParseEnum(s, kCullLUT, D3D12_CULL_MODE_BACK);
}

D3D12_COMPARISON_FUNC PSOList::ParseCmpFunc(const std::string& s)
{
	return ParseEnum(s, kCmpLUT, D3D12_COMPARISON_FUNC_LESS_EQUAL);
}

std::string PSOList::GetRootParamInfo(int index, const RootParamSpec& spec)
{
	std::stringstream ss;
	ss << spec.name <<"  [" << index << "] ";

	switch (spec.type)
	{
		case ERootParamType::Constants:
			ss << "RootConstants (b" << spec.baseRegister << ", space" << spec.registerSpace << ") | Num32Bit: " << spec.numConstants;
			break;
		case ERootParamType::CBV:
			ss << "RootCBV       (b" << spec.baseRegister << ", space" << spec.registerSpace << ")";
			break;
		case ERootParamType::SRV:
			ss << "RootSRV       (t" << spec.baseRegister << ", space" << spec.registerSpace << ")";
			break;
		case ERootParamType::UAV:
			ss << "RootUAV       (u" << spec.baseRegister << ", space" << spec.registerSpace << ")";
			break;
		case ERootParamType::Table:
		{
			ss << "DescriptorTable | Ranges: " << spec.ranges.size()<<"\n";
			for (const auto& r : spec.ranges)
			{
				std::string typeStr;
				char regChar = '?';
				switch (r.type) {
					case ERootParamType::CBV: typeStr = "CBV"; regChar = 'b'; break;
					case ERootParamType::SRV: typeStr = "SRV"; regChar = 't'; break;
					case ERootParamType::UAV: typeStr = "UAV"; regChar = 'u'; break;
				}
				ss << "\t" << r.name << " [" << typeStr << "(" << regChar << r.baseRegister << ", cnt : " << r.count << ")]";
			}
		}
		break;
		default:
			ss << "Unknown";
			break;
	}
	return ss.str();
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