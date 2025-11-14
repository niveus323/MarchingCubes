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

	// Initialize Camera Resource
	{
		static const uint32_t cameraBufferSize = AlignUp(sizeof(CameraConstants), CB_ALIGN);

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(cameraBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_cameraCB.ReleaseAndGetAddressOf())
		));
		NAME_D3D12_OBJECT(m_cameraCB);

		// Map & CreateUploadBuffer Constant Buffer
		CD3DX12_RANGE readRAnge(0, 0);
		ThrowIfFailed(m_cameraCB->Map(0, &readRAnge, reinterpret_cast<void**>(&m_cameraCBMapped)));
	}

	// Initialize Light Resource
	{
		const uint32_t rawBytes = sizeof(LightConstantsHeader) + kMaxLights * sizeof(Light);

		m_lightCBSize = AlignUp(rawBytes, CB_ALIGN);

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(m_lightCBSize);
		ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_lightCB.ReleaseAndGetAddressOf())));

		ThrowIfFailed(m_lightCB->Map(0, nullptr, reinterpret_cast<void**>(&m_lightCBMapped)));
	}
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

	if (m_lightCB)
	{
		m_lightCB->Unmap(0, nullptr);
		m_lightCBMapped = nullptr;
	}

	if (m_cameraCB)
	{
		m_cameraCB->Unmap(0, nullptr);
		m_cameraCBMapped = nullptr;
	}
}

// 업로드 준비
void RenderSystem::PrepareRender(ID3D12GraphicsCommandList* cmd, UploadContext& uploadContext, const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex)
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
				uploadContext.UploadObjectConstants(frameIndex, buf, cb);
			}
		}

		for (IDrawable* drawable : bucket.staticItems)
		{
			// Per-Frame CB 업데이트
			if (GeometryBuffer* buf = drawable->GetGPUBuffer())
			{
				ObjectConstants cb = drawable->GetObjectConstants();
				uploadContext.UploadObjectConstants(frameIndex, buf, cb);
			}
		}
	}

	UploadCameraConstants(cameraData);
	UploadLightConstants(lightData);
}

// 렌더링 
void RenderSystem::RenderFrame(ID3D12GraphicsCommandList* cmd)
{
	// Bind Common RootCBV
	cmd->SetGraphicsRootConstantBufferView(0, m_cameraCB->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(2, m_lightCB->GetGPUVirtualAddress());

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

void RenderSystem::UploadCameraConstants(const CameraConstants& cameraData)
{
	memcpy(m_cameraCBMapped, &cameraData, sizeof(CameraConstants));

	/*BufferHandle camHandle;
	m_gpuAllocator->Alloc(m_device, AllocDesc{ .kind = AllocDesc::Kind::CB, .lifetime = AllocDesc::LifeTime::LONG, .size = sizeof(CameraConstants) ,.align = CB_ALIGN, .owner = "Camera" }, camHandle);
	memcpy(camHandle.cpuPtr, &cameraData, sizeof(CameraConstants));
	m_gpuAllocator->FreeLater(camHandle, ...);*/
}

void RenderSystem::UploadLightConstants(const LightBlobView& lightData)
{
	assert(lightData.size <= m_lightCBSize && "Excetion : LightData Overflowing");
	const size_t blobSizeToCopy = (lightData.size <= m_lightCBSize) ? lightData.size : static_cast<size_t>(m_lightCBSize);

	// m_lightCBMapped는 Init에서 Map해둔 업로드 힙 메모리 시작 주소
	std::memcpy(m_lightCBMapped, lightData.data, blobSizeToCopy);
	size_t remain = m_lightCBSize - blobSizeToCopy;
	if (remain > 0)
	{
		std::memset(m_lightCBMapped + blobSizeToCopy, 0, remain);
	}
}