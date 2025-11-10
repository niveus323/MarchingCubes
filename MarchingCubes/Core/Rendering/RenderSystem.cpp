#include "pch.h"
#include "RenderSystem.h"
#include "Core/DataStructures/Data.h"

RenderSystem::RenderSystem(ID3D12Device* device, ID3D12RootSignature* rootSignature, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const std::vector<std::wstring>& psoFiles)
{
	m_gpuAllocator = std::make_unique<GpuAllocator>(device);
	m_staticBufferRegistry = std::make_unique<StaticBufferRegistry>(device);
	std::vector<PSOSpec> mergedSpecs;
	for (auto& psoFile : psoFiles)
	{
		int schema = 0;
		std::wstring fileDir = GetFullPath(AssetType::Default, L"PSO/") + psoFile;
		auto specs = LoadPSOJsonResolved(fileDir.c_str(), &schema);
		mergedSpecs.insert(mergedSpecs.end(), specs.begin(), specs.end());
	}

	PSOList::BuildContext ctx{};
	ctx.device = device;
	ctx.root = rootSignature;
	ctx.inputLayout = inputLayout;

	m_psoList = std::make_unique<PSOList>(ctx, mergedSpecs);
	m_buckets.resize(m_psoList->Count());

	m_bundleRecorder = std::make_unique<BundleRecorder>(device, rootSignature, m_psoList.get(), 2);
	m_uploadContext = std::make_unique<UploadContext>(device, m_gpuAllocator.get(), m_staticBufferRegistry.get());
	m_passEnabled.resize(m_psoList->Count(), true);

	// Initialize Camera Resource
	{
		static const UINT cameraBufferSize = AlignUp(sizeof(CameraConstants), CB_ALIGN);

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
		const UINT rawBytes = sizeof(LightConstantsHeader) + kMaxLights * sizeof(Light);

		m_lightCBSize = AlignUp(rawBytes, CB_ALIGN);

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(m_lightCBSize);
		ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_lightCB.ReleaseAndGetAddressOf())));

		ThrowIfFailed(m_lightCB->Map(0, nullptr, reinterpret_cast<void**>(&m_lightCBMapped)));
	}
}

RenderSystem::~RenderSystem()
{
	for (auto& bucket : m_buckets)
	{
		bucket.dynamicItems.clear();
		bucket.staticItem.objects.clear();
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
void RenderSystem::PrepareRender(ID3D12GraphicsCommandList* cmd)
{
	// 이번 프레임에 할당 가능한 공간 체크
	m_uploadContext->ReclaimCompleted(m_completedFenceValue);
	m_gpuAllocator->Reclaim(m_completedFenceValue);

	// 동적 오브젝트 업로드
	for (auto& bucket : m_buckets)
	{
		for (auto& dyn : bucket.dynamicItems)
		{
			IDrawable* drawable = dyn.object;
			if (drawable == nullptr)
			{
				Log::Print("RenderSystem", "Invalid Drawable in Dynamic Item!!!!");
				continue;
			}

			// lazy-stage
			if (drawable->IsUploadPending())
			{
				m_uploadContext->UploadDrawable(drawable, m_completedFenceValue);
			}

			// Per-Frame CB 업데이트
			if (GeometryBuffer* buf = drawable->GetGPUBuffer())
			{
				ObjectConstants cb = drawable->GetObjectConstants();
				m_uploadContext->UploadObjectConstants(buf, cb);
			}
		}

		for (IDrawable* drawable : bucket.staticItem.objects)
		{
			if (drawable == nullptr)
			{
				Log::Print("RenderSystem", "Invalid Drawable in Static Item!!!!");
				continue;
			}
			// lazy-stage
			if (drawable->IsUploadPending())
			{
				m_uploadContext->UploadDrawable(drawable, m_completedFenceValue);
			}
			// Per-Frame CB 업데이트
			if (GeometryBuffer* buf = drawable->GetGPUBuffer())
			{
				ObjectConstants cb = drawable->GetObjectConstants();
				m_uploadContext->UploadObjectConstants(buf, cb);
			}
		}
	}

	// Commit
	m_uploadContext->Execute(cmd);
}

// 렌더링 
void RenderSystem::RenderFrame(ID3D12GraphicsCommandList* cmd, const CameraConstants& cameraData, const LightBlobView& lightData)
{
	UploadCameraConstants(cameraData);
	UploadLightConstants(lightData);

	BindCommonResources(cmd);

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

		// 정적 번들 실행
		if (ID3D12GraphicsCommandList* bundle = bucket.staticItem.bundle)
		{
			cmd->ExecuteBundle(bundle);
		}

		// 동적 오브젝트 렌더
		for (auto& item : bucket.dynamicItems)
			RecordDrawItem(cmd, item.object);
	}
}

PendingDeleteItem RenderSystem::CleanUp(UINT64 fenceValue)
{
	PendingDeleteItem pd{};
	if (!m_toDeletesContainer.empty())
	{
		pd.fenceValue = fenceValue;
		pd.resources = std::move(m_toDeletesContainer);
	}
	return pd;
}

bool RenderSystem::IsDynamicRegistered(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];

	for (auto& item : bucket.dynamicItems)
	{
		if (item.object == drawable) return true;
	}

	return false;
}

// 정적 오브젝트 등록
bool RenderSystem::RegisterStatic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];
	for (auto& object : bucket.staticItem.objects)
	{
		if (object == drawable)
		{
			Log::Print("RenderSystem", "RegisterStatic : Already Registered Object!!!!");
			return false;
		}
	}
	bucket.staticItem.objects.push_back(drawable);

	// VB/IB 스테이징
	m_uploadContext->UploadStatic(drawable, m_completedFenceValue);
	// 정적 번들은 미리 CB를 할당 및 업로드해둬야함
	m_uploadContext->UploadObjectConstants(drawable->GetGPUBuffer(), drawable->GetObjectConstants());

	// 번들 재생성
	bucket.staticItem.bundle = m_bundleRecorder->CreateBundleFor(bucket.staticItem.objects, psoName);
	return true;
}

// 동적 오브젝트 등록
bool RenderSystem::RegisterDynamic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];

	for (auto& item : bucket.dynamicItems)
	{
		if (item.object == drawable)
		{
			Log::Print("RenderSystem", "RegisterDynamic : Already Registered Object!!!!");
			return false;
		}
	}

	DynamicRenderItem dyn{};
	dyn.object = drawable;
	dyn.object->SetUploadPending(true);

	bucket.dynamicItems.push_back(std::move(dyn));
	return true;
}

bool RenderSystem::UnRegisterStatic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];
	size_t result = std::erase_if(bucket.staticItem.objects, [drawable](IDrawable* object) { return drawable == object; });
	return result > 0;
}

bool RenderSystem::UnRegisterDynamic(IDrawable* drawable, const std::string& psoName)
{
	int psoIdx = GetPSOIndex(psoName);
	auto& bucket = m_buckets[psoIdx];
	size_t result = std::erase_if(bucket.dynamicItems, [drawable](const DynamicRenderItem& item) { return drawable == item.object; });

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
		for (auto& item : bucket.dynamicItems)
		{
			if (item.object == drawable)
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

void RenderSystem::CallWhenFenceSignaled(UINT64 fenceValue)
{
	m_completedFenceValue = fenceValue;

	// 이번 프레임에 할당했던 요소들에 대해 완료 대기를 걸어둔다 
	m_uploadContext->TrackPendingAllocations(m_completedFenceValue);
}

void RenderSystem::UploadCameraConstants(const CameraConstants& cameraData)
{
	memcpy(m_cameraCBMapped, &cameraData, sizeof(CameraConstants));
}

void RenderSystem::UploadLightConstants(const LightBlobView& lightData)
{
#ifdef _DEBUG
	assert(lightData.size <= m_lightCBSize && "Excetion : LightData Overflowing");
#endif
	const size_t blobSizeToCopy = (lightData.size <= m_lightCBSize) ? lightData.size : static_cast<size_t>(m_lightCBSize);

	// m_lightCBMapped는 Init에서 Map해둔 업로드 힙 메모리 시작 주소
	std::memcpy(m_lightCBMapped, lightData.data, blobSizeToCopy);
	size_t remain = m_lightCBSize - blobSizeToCopy;
	if (remain > 0)
	{
		std::memset(m_lightCBMapped + blobSizeToCopy, 0, remain);
	}
}

void RenderSystem::BindCommonResources(ID3D12GraphicsCommandList* cmd)
{
	cmd->SetGraphicsRootConstantBufferView(0, m_cameraCB->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(3, m_lightCB->GetGPUVirtualAddress());
}
