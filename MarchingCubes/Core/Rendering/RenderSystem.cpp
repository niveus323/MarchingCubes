#include "pch.h"
#include "RenderSystem.h"
#include "Core/DataStructures/Data.h"
#include <unordered_map>

RenderSystem::RenderSystem(RenderSystemInitInfo init_info) :
	m_info(std::move(init_info))
{
	assert(m_info.device && "RenderSystem : InValid Device!!!!");
	assert(m_info.rootSignature && "RenderSystem : InValid RootSignature!!!!");

	ID3D12Device* device = m_info.device;
	ID3D12RootSignature* rootSignature = m_info.rootSignature;

	std::vector<PSOSpec> mergedSpecs;
	for (auto& psoFile : m_info.psoFiles)
	{
		int schema = 0;
		std::filesystem::path filePath = GetFullPath(AssetType::Default, L"PSO") / psoFile;
		auto specs = LoadPSOJsonResolved(filePath.c_str(), &schema);
		mergedSpecs.insert(mergedSpecs.end(), specs.begin(), specs.end());
	}

	D3D12_INPUT_LAYOUT_DESC inputLayout{};
	inputLayout.pInputElementDescs = m_info.inputElements.data();
	inputLayout.NumElements = static_cast<UINT>(m_info.inputElements.size());
	PSOList::BuildContext ctx{};
	ctx.device = device;
	ctx.root = rootSignature;
	ctx.inputLayout = inputLayout;

	m_psoList = std::make_unique<PSOList>(ctx, mergedSpecs);
	m_buckets.resize(m_psoList->Count());

	m_bundleRecorder = std::make_unique<BundleRecorder>(device, rootSignature, m_psoList.get(), 2);
}

RenderSystem::RenderSystem(ID3D12Device* device, ID3D12RootSignature* rootSignature, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const std::vector<std::wstring>& psoFiles) :
	RenderSystem(RenderSystemInitInfo{
		.device = device,
		.rootSignature = rootSignature,
		.inputElements = std::vector<D3D12_INPUT_ELEMENT_DESC>(inputLayout.pInputElementDescs, inputLayout.pInputElementDescs + inputLayout.NumElements),
		.psoFiles = psoFiles })
{
}

RenderSystem::~RenderSystem()
{
	for (auto& bucket : m_buckets)
	{
		bucket.renderItems.clear();
	}
}

void RenderSystem::PrepareRender(_In_ UploadContext* uploadContext, _In_ DescriptorAllocator* descriptorAllocator, const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex)
{
	uploadContext->UploadContstants(frameIndex, &cameraData, sizeof(CameraConstants), m_cameraBuf);
	
	const uint32_t lightCBSize = AlignUp(sizeof(LightConstantsHeader) + kMaxLights * sizeof(Light), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	const uint32_t blobSizeToCopy = (lightData.size <= lightCBSize) ? lightData.size : lightCBSize;
	uploadContext->UploadContstants(frameIndex, lightData.data, blobSizeToCopy, m_lightsBuf);
	uint32_t lightsSlot = descriptorAllocator->AllocateDynamic(frameIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE lightsCpu = descriptorAllocator->GetDynamicCpu(frameIndex, lightsSlot);
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = m_lightsBuf.gpuVA;
	desc.SizeInBytes = AlignUp(blobSizeToCopy, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	m_info.device->CreateConstantBufferView(&desc, lightsCpu);
	m_lightsGpu = descriptorAllocator->GetDynamicGpu(frameIndex, lightsSlot);
}

// 렌더링 
void RenderSystem::RenderFrame(_In_ ID3D12GraphicsCommandList* cmd, _In_ UploadContext* uploadContext)
{
	// Bind Common RootCBV
	cmd->SetGraphicsRootConstantBufferView(0, m_cameraBuf.gpuVA);
	cmd->SetGraphicsRootDescriptorTable(2, m_lightsGpu);

	// PSO 별로 Draw Command 실행
	for (int i = 0; i < m_psoList->Count(); ++i) 
	{
		auto* pso = m_psoList->Get(i);
		auto& bucket = m_buckets[i];
		if (!pso || bucket.renderItems.empty()) continue;
		cmd->SetPipelineState(pso);
		for (auto& item : bucket.renderItems)
		{
			// Object CB 세팅
			ObjectConstants objConsts{
				.materialIndex = item.materialIndex
			};

			// row-major -> column-major 변환
			XMMATRIX worldMatrix = XMLoadFloat4x4(&item.worldMatrix);
			XMStoreFloat4x4(&objConsts.worldMatrix, XMMatrixTranspose(worldMatrix));
			XMStoreFloat4x4(&objConsts.worldInvMatrix, XMMatrixInverse(nullptr, worldMatrix));
			
			BufferHandle cbHandle;
			uploadContext->UploadContstants(0, &objConsts, sizeof(ObjectConstants), cbHandle);
			cmd->SetGraphicsRootConstantBufferView(1, cbHandle.gpuVA);
			DrawItem(cmd, item);
		}
		bucket.renderItems.clear();
	}
}

bool RenderSystem::SubmitRenderItem(const RenderItem& item, std::string_view psoName)
{
	std::string finalPSO(psoName);
	if (!m_psoOverrides.empty())
	{
		auto it = m_psoOverrides.find(finalPSO);
		if (it != m_psoOverrides.end())
		{
			finalPSO = it->second;
		}
	}

	SubmitToBucket(finalPSO, item);
	if (!m_psoExtensions.empty())
	{
		auto range = m_psoExtensions.equal_range(std::string(psoName));
		for (auto it = range.first; it != range.second; ++it)
		{
			const std::string& extPSO = it->second;
			SubmitToBucket(extPSO, item);
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

void RenderSystem::SubmitToBucket(std::string_view psoName, const RenderItem& item)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];

	for (auto& iter : bucket.renderItems)
	{
		if (iter.meshBuffer == item.meshBuffer)
		{
			Log::Print("RenderSystem", "SubmitRenderItem : Already Registered Object!!!!");
			return;
		}
	}
	bucket.renderItems.push_back(item);
}
