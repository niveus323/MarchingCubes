#include "pch.h"
#include "RenderSystem.h"
#include "Core/DataStructures/Data.h"

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
		std::wstring fileDir = GetFullPath(AssetType::Default, L"PSO/") + psoFile;
		auto specs = LoadPSOJsonResolved(fileDir.c_str(), &schema);
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
	m_passEnabled.resize(m_psoList->Count(), true);
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
		bucket.dynamicItems.clear();
		bucket.staticItems.clear();
	}
}

void RenderSystem::PrepareRender(_In_ UploadContext* uploadContext, _In_ DescriptorAllocator* descriptorAllocator, const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex)
{
	// Upload Per-Object Constants
	for (int i = 0; i < m_buckets.size(); ++i)
	{
		auto& bucket = m_buckets[i];
		for (auto& drawable : bucket.dynamicItems)
		{
			// Per-Frame CB 업데이트
			if (GeometryBuffer* buf = drawable->GetGPUBuffer())
			{
				ObjectConstants cb = drawable->GetObjectConstants();
				uploadContext->UploadObjectConstants(frameIndex, buf, cb);
			}
		}

		for (IDrawable* drawable : bucket.staticItems)
		{
			// Per-Frame CB 업데이트
			if (GeometryBuffer* buf = drawable->GetGPUBuffer())
			{
				ObjectConstants cb = drawable->GetObjectConstants();
				uploadContext->UploadObjectConstants(frameIndex, buf, cb);
			}
		}
	}

	uploadContext->UploadContstants(frameIndex, &cameraData, sizeof(CameraConstants), m_cameraBuf);
	
	const uint32_t lightCBSize = AlignUp(sizeof(LightConstantsHeader) + kMaxLights * sizeof(Light), CB_ALIGN);
	const uint32_t blobSizeToCopy = (lightData.size <= lightCBSize) ? lightData.size : lightCBSize;
	uploadContext->UploadContstants(frameIndex, lightData.data, blobSizeToCopy, m_lightsBuf);
	uint32_t lightsSlot = descriptorAllocator->AllocateDynamic(frameIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE lightsCpu = descriptorAllocator->GetDynamicCpu(frameIndex, lightsSlot);
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = m_lightsBuf.gpuVA;
	desc.SizeInBytes = static_cast<UINT>(m_lightsBuf.size);
	m_info.device->CreateConstantBufferView(&desc, lightsCpu);
	m_lightsGpu = descriptorAllocator->GetDynamicGpu(frameIndex, lightsSlot);
}

// 렌더링 
void RenderSystem::RenderFrame(ID3D12GraphicsCommandList* cmd)
{
	// Bind Common RootCBV
	cmd->SetGraphicsRootConstantBufferView(0, m_cameraBuf.gpuVA);
	//cmd->SetGraphicsRootConstantBufferView(2, m_lightsBuf.gpuVA);
	cmd->SetGraphicsRootDescriptorTable(2, m_lightsGpu);

	// PSO 별로 Draw Command 실행
	for (int i = 0; i < m_psoList->Count(); ++i) {
		auto* pso = m_psoList->Get(i);
		auto& bucket = m_buckets[i];
		if (!pso || !m_passEnabled[i]) continue;

#ifdef _DEBUG
		cmd->SetPipelineState(m_wireViewEnabled ? m_psoList->Get(m_psoList->IndexOf("Wire")) : pso);
#else
		cmd->SetPipelineState(pso);
#endif // _DEBUG

		for (auto& item : bucket.staticItems)
		{
			RecordDrawItem(cmd, item->GetDrawBinding());
		}

		// 동적 오브젝트 렌더
		for (auto& item : bucket.dynamicItems)
		{
			RecordDrawItem(cmd, item->GetDrawBinding());
		}
	}
}

bool RenderSystem::IsDynamicRegistered(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];

	for (auto& object : bucket.dynamicItems)
	{
		if (object == drawable) return true;
	}

	return false;
}

// 정적 오브젝트 등록
bool RenderSystem::RegisterStatic(IDrawable* drawable, const std::string& psoName, uint32_t frameIndex)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];
	for (auto& object : bucket.staticItems)
	{
		if (object == drawable)
		{
			Log::Print("RenderSystem", "RegisterStatic : Already Registered Object!!!!");
			return false;
		}
	}
	bucket.staticItems.push_back(drawable);
	return true;
}

// 동적 오브젝트 등록
bool RenderSystem::RegisterDynamic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];

	for (auto& object : bucket.dynamicItems)
	{
		if (object == drawable)
		{
			Log::Print("RenderSystem", "RegisterDynamic : Already Registered Object!!!!");
			return false;
		}
	}
	drawable->SetUploadPending(true);
	bucket.dynamicItems.push_back(drawable);
	return true;
}

bool RenderSystem::UnRegisterStatic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];
	size_t result = std::erase_if(bucket.staticItems, [drawable](IDrawable* object) { return drawable == object; });
	return result > 0;
}

bool RenderSystem::UnRegisterDynamic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];
	size_t result = std::erase_if(bucket.dynamicItems, [drawable](const IDrawable* item) { return drawable == item; });

	if (GeometryBuffer* buffer = drawable->GetGPUBuffer())
		buffer->ReleaseGPUResources();

	return result > 0;
}

// 동적 오브젝트 갱신
bool RenderSystem::UpdateDynamic(IDrawable* drawable, const GeometryData& data)
{
	if (!drawable) return false;

	// 동적 오브젝트 리스트에 있는지 체크
	bool found = false;
	for (auto& bucket : m_buckets)
	{
		for (auto& object : bucket.dynamicItems)
		{
			if (object == drawable)
			{
				found = true;
				break;
			}
		}
		if (found) break;
	}

	if (found)
	{
		drawable->SetCPUData(data);
		drawable->SetUploadPending(true);
	}

	// 없었다면 등록하도록 유도
	return found;
}