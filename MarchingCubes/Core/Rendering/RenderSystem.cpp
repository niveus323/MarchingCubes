#include "pch.h"
#include "RenderSystem.h"
#include "Core/DataStructures/Data.h"
#include "Core/Assets/TextureRegistry.h"
#include "Core/Assets/MeshRegistry.h"
#include "Core/Assets/Material/MaterialRegistry.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include <unordered_map>
#include <numeric>
#include <algorithm>

RenderSystem::RenderSystem(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElems, const std::vector<std::wstring>& psoFiles) :
	m_inputElements(inputElems),
	m_psoFiles(psoFiles)
{
	ID3D12Device* device = EngineCore::GetDevice();
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();

	// PipelineState 객체들 생성
	{
		std::vector<RootSignatureSpec> rsSpecs;
		std::vector<PSOSpec> psoSpecs;
		for (auto& psoFile : m_psoFiles)
		{
			int schema = 0;
			std::filesystem::path filePath = GetFullPath(AssetType::Default, L"PSO") / psoFile;
			auto data = LoadPipelineBundle(filePath.c_str());
			rsSpecs.insert(rsSpecs.end(), data.rsSpecs.begin(), data.rsSpecs.end());
			psoSpecs.insert(psoSpecs.end(), data.psoSpecs.begin(), data.psoSpecs.end());
		}

		PSOList::BuildContext ctx{
			.device = device,
			.inputLayout = D3D12_INPUT_LAYOUT_DESC{
				.pInputElementDescs = m_inputElements.data(),
				.NumElements = static_cast<UINT>(m_inputElements.size())
			}
		};

		m_psoList = std::make_unique<PSOList>(ctx, psoSpecs, rsSpecs);
	}
	m_bundleRecorder = std::make_unique<BundleRecorder>(device, m_psoList.get(), 2);

	//Registry
	m_textureRegistry = std::make_unique<TextureRegistry>();
	m_materialRegistry = std::make_unique<MaterialRegistry>(m_textureRegistry.get());
	m_meshRegistry = std::make_unique<MeshRegistry>();

}

RenderSystem::~RenderSystem()
{
	m_renderQueue.clear();
}

void RenderSystem::SyncGpu(ID3D12GraphicsCommandList* cmd)
{
	m_textureRegistry->SyncGpu(cmd);
	m_materialRegistry->SyncGpu(cmd);
}

// PSO의 공용 CB를 업로드
void RenderSystem::PrepareRender(const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex)
{
	UploadContext* uploadContext = EngineCore::GetUploadContext();
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();

	uploadContext->UploadConstants(&cameraData, sizeof(CameraConstants), m_cameraBuf);

	const uint32_t lightCBSize = AlignUp(sizeof(LightConstantsHeader) + kMaxLights * sizeof(Light), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	const uint32_t blobSizeToCopy = (lightData.size <= lightCBSize) ? lightData.size : lightCBSize;
	uploadContext->UploadConstants(lightData.data, blobSizeToCopy, m_lightsBuf);

	uint32_t lightsSlot = descriptorAllocator->AllocateDynamic(frameIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE lightsCpu = descriptorAllocator->GetDynamicCpu(frameIndex, lightsSlot);
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = m_lightsBuf.gpuVA;
	desc.SizeInBytes = AlignUp(blobSizeToCopy, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	EngineCore::GetDevice()->CreateConstantBufferView(&desc, lightsCpu);
	m_lightsGpu = descriptorAllocator->GetDynamicGpu(frameIndex, lightsSlot);

	// 렌더 큐 사전 정렬
	std::sort(m_renderQueue.begin(), m_renderQueue.end(), [](const auto& a, const auto& b) { return a.sortKey < b.sortKey; });
}

// 렌더링 
void RenderSystem::RenderFrame(ID3D12GraphicsCommandList* cmd)
{
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorAllocator->GetCbvSrvUavHeap(), descriptorAllocator->GetSamplerHeap(0) };
	cmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	uint16_t currentRSIndex = 0xFFFF;
	uint16_t currentPSOIndex = 0xFFFF;
	for (const auto& entry : m_renderQueue)
	{
		uint16_t nextPSOIndex = entry.psoIndex;
		// PSO 교체 확인
		if (currentPSOIndex != nextPSOIndex)
		{
			auto pipelineData = m_psoList->Get(nextPSOIndex);

			// RS 교체 확인
			uint16_t nextRSIndex = m_psoList->GetRSIndex(nextPSOIndex);
			if (currentRSIndex != nextRSIndex)
			{
				cmd->SetGraphicsRootSignature(pipelineData.rs);
				currentRSIndex = nextRSIndex;

				// Bind Common Resources
				cmd->SetGraphicsRootConstantBufferView(0, m_cameraBuf.gpuVA);
				cmd->SetGraphicsRootConstantBufferView(2, m_lightsBuf.gpuVA);
				m_materialRegistry->BindDescriptorTable(cmd);
				m_textureRegistry->BindDescriptorTable(cmd);
			}
			cmd->SetPipelineState(pipelineData.pso);
			currentPSOIndex = nextPSOIndex;
		}
		DrawItem(cmd, entry.item); //MeshBuffer는 있는데 vb, ib의 res 객체가 null 인 케이스 존재.
	}
	m_renderQueue.clear();
}

bool RenderSystem::SubmitRenderItem(const RenderItem& item, std::string_view psoName)
{
	std::string finalPSO(psoName);
	if (!m_psoOverrides.empty()) // PSO 덮어쓰기 기능을 사용하는 경우 PSO 변경
	{
		auto it = m_psoOverrides.find(finalPSO);
		if (it != m_psoOverrides.end())
		{
			finalPSO = it->second;
		}
	}
	SubmitToQueue(finalPSO, item);

	// 추가 PSO 사용 여부 확인
	if (!m_psoExtensions.empty())
	{
		auto range = m_psoExtensions.equal_range(std::string(psoName));
		for (auto it = range.first; it != range.second; ++it)
		{
			const std::string& extPSO = it->second;
			SubmitToQueue(extPSO, item);
		}
	}

	return true;
}

void RenderSystem::RemovePSOExtension(const std::string& from, const std::string& to)
{
	auto range = m_psoExtensions.equal_range(from);
	for (auto iter = range.first; iter != range.second; ++iter)
	{
		if (iter->second == to)
		{
			m_psoExtensions.erase(iter);
			break;
		}
	}
}

bool RenderSystem::SubmitToQueue(std::string_view psoName, const RenderItem& item)
{
	int psoIndexInt = m_psoList->IndexOf(psoName);
	if (psoIndexInt == -1) return false;
	uint16_t psoIndex = static_cast<uint16_t>(psoIndexInt);
	uint16_t rsIndex = m_psoList->GetRSIndex(psoIndex);

	// TODO : 투명 오브젝트 적용 시 Transparent 필드 추가 및 처리
	// bool bTransparent = ...;

	// SortKey 생성
	uint64_t key = GenerateSortKey(rsIndex, psoIndex, item);
	m_renderQueue.push_back({ key, psoIndex, item });
	return true;
}

uint64_t RenderSystem::GenerateSortKey(uint16_t rsIndex, uint16_t psoIndex, const RenderItem& item)
{
	uint64_t key = 0;

	// TODO : 투명 오브젝트들의 Key 별도 처리
	//if (bTransparent)
	//{ ... }

	// RS -> PSO -> Material 정렬
	key |= ((uint64_t)rsIndex << 48);
	key |= ((uint64_t)psoIndex << 32);
	key |= ((uint64_t)item.materialIndex);
	return key;
}
