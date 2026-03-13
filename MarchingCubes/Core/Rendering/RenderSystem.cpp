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

RenderSystem::RenderSystem(const std::vector<std::wstring>& psoFiles) :
	m_psoFiles(psoFiles)
{
	ID3D12Device* device = EngineCore::GetDevice();
	DescriptorAllocator* descriptorAllocator = EngineCore::GetDescriptorAllocator();

	// PipelineState 객체들 생성
	{
		std::vector<RootSignatureSpec> rsSpecs;
		std::vector<PSOSpec> psoSpecs;
		std::vector<InputLayoutSpec> iaSpecs;
		for (auto& psoFile : m_psoFiles)
		{
			int schema = 0;
			std::filesystem::path filePath = GetFullPath(AssetType::Default, L"PSO") / psoFile;
			auto data = LoadPipelineBundle(filePath.c_str());
			rsSpecs.insert(rsSpecs.end(), data.rsSpecs.begin(), data.rsSpecs.end());
			psoSpecs.insert(psoSpecs.end(), data.psoSpecs.begin(), data.psoSpecs.end());
			iaSpecs.insert(iaSpecs.end(), data.iaSpecs.begin(), data.iaSpecs.end());
		}

		m_psoList = std::make_unique<PSOList>(device, psoSpecs, rsSpecs, iaSpecs);
	}
	m_bundleRecorder = std::make_unique<BundleRecorder>(device, m_psoList.get(), 2);

	//Registry
	m_textureRegistry = std::make_unique<TextureRegistry>();
	m_materialRegistry = std::make_unique<MaterialRegistry>(m_textureRegistry.get());
	m_meshRegistry = std::make_unique<MeshRegistry>();

	RegisterIDPsoMapping("Filled", "IDPass_Mesh");
	RegisterIDPsoMapping("EditorBillboard", "IDPass_Billboard");

	// HitProxy Readback 버퍼 생성(크기가 고정되어 있으므로 초기화 단계에서 생성
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_hitProxyReadback)
	);

	m_renderItems.resize(100000);
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

	// 렌더 큐 사전 정렬
	std::sort(m_renderQueue.begin(), m_renderQueue.end(), [](const auto& a, const auto& b) { return a.sortKey < b.sortKey; });
}

// 렌더링 
void RenderSystem::RenderFrame(ID3D12GraphicsCommandList* cmd)
{
	auto device = EngineCore::GetDevice();
	auto descriptorAllocator = EngineCore::GetDescriptorAllocator();
	// [패스 1] HitProxy On-Demand 렌더링
	if (m_hitProxyRT)
	{
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_hitProxyRT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

		auto rtvCpu = descriptorAllocator->GetRTVCpu(m_hitProxyRTV);
		auto dsvCpu = m_currentDSV;

		cmd->OMSetRenderTargets(1, &rtvCpu, FALSE, &dsvCpu);

		// NOTE: 시각적 디버깅을 위해 전체 화면 렌더링 (추후 최적화 시 1x1 ScissorRect 적용)
		const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		cmd->ClearRenderTargetView(rtvCpu, clearColor, 0, nullptr);
		cmd->ClearDepthStencilView(dsvCpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		ExecuteQueue(cmd, true);

		// Readback을 위해 SRV로 전환
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_hitProxyRT.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		if (m_bPickingRequested)
		{
			// 마우스 영역을 중심으로 1x1 픽셀을 Readback
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_hitProxyRT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE));

			// Footprint 설정 (가로 1, 세로 1, 포맷 R8G8B8A8)
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {
				.Offset = 0,
				.Footprint{
					.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
					.Width = 1,
					.Height = 1,
					.Depth = 1,
					.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
				}
			};
			
			CD3DX12_TEXTURE_COPY_LOCATION dest(m_hitProxyReadback.Get(), footprint);
			CD3DX12_TEXTURE_COPY_LOCATION src(m_hitProxyRT.Get(), 0);
			D3D12_BOX srcBox = { m_pickX, m_pickY, 0, m_pickX + 1, m_pickY + 1, 1 };
			cmd->CopyTextureRegion(&dest, 0, 0, 0, &src, &srcBox);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_hitProxyRT.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			Log::Print(ELogVerbosity::Message, "RenderSystem", "Pick Area({}, {})", m_pickX, m_pickY);
			
			m_bPickingRequested = false;
		}
	}	

	// [패스 2] 메인 렌더링
	cmd->OMSetRenderTargets(1, &m_currentRTV, FALSE, &m_currentDSV);

	// 메인 렌더링을 위해 Clear
	const float clearColor[] = { 0.0f, 0.0f, 0.2f, 1.0f };
	cmd->ClearRenderTargetView(m_currentRTV, clearColor, 0, nullptr);
	cmd->ClearDepthStencilView(m_currentDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	ExecuteQueue(cmd, false);
	m_renderQueue.clear();
	m_allocatedItemCount = 0;
}

void RenderSystem::SubmitRenderItem(const RenderItem* item, std::string_view psoName)
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
	bool result = SubmitToQueue(finalPSO, item);
	if (!result) return; // 이미 제출에 실패했으므로 추가 PSO도 작업할 필요 없음

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

void RenderSystem::SetViewport(float x, float y, float width, float height)
{
	m_viewport.TopLeftX = x;
	m_viewport.TopLeftY = y;
	m_viewport.Width = width;
	m_viewport.Height = height;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissorRect.left = static_cast<LONG>(x);
	m_scissorRect.top = static_cast<LONG>(y);
	m_scissorRect.right = static_cast<LONG>(x + width);
	m_scissorRect.bottom = static_cast<LONG>(y + height);

	Log::Print(ELogVerbosity::Message, "RenderSystem", "Viewport : {}, {}.    ScissorRect : ({}, {}, {}, {})", m_viewport.Width, m_viewport.Height, m_scissorRect.left, m_scissorRect.top, m_scissorRect.right, m_scissorRect.bottom);
}

void RenderSystem::SetOutputTarget(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	m_currentRenderTarget = renderTarget;
	m_currentRTV = rtv;
	m_currentDSV = dsv;
}

void RenderSystem::RegisterIDPsoMapping(const std::string& basePsoName, const std::string& idPsoName)
{
	int baseIdx = m_psoList->IndexOf(basePsoName);
	int hitProxyIdx = m_psoList->IndexOf(idPsoName);
	if (baseIdx != -1 && hitProxyIdx != -1)
	{
		m_psoToIDPsoMap[static_cast<uint16_t>(baseIdx)] = static_cast<uint16_t>(hitProxyIdx);
	}
}

void RenderSystem::CreateHitProxyTarget(uint32_t width, uint32_t height)
{
	width = std::max(1u, width);
	height = std::max(1u, height);

	if (m_hitProxyRT != nullptr && m_hitProxyWidth == width && m_hitProxyHeight == height)
	{
		return;
	}

	m_hitProxyWidth = width;
	m_hitProxyHeight = height;

	auto device = EngineCore::GetDevice();
	auto allocator = EngineCore::GetDescriptorAllocator();

	if (m_hitProxyRTV == UINT32_MAX) m_hitProxyRTV = allocator->AllocateRTV();
	if (m_hitProxySRV == UINT32_MAX) m_hitProxySRV = allocator->AllocateStaticSlot();

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_hitProxyWidth, m_hitProxyHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	// 배경을 검은색(ID 0, 알파 0)으로 초기화
	D3D12_CLEAR_VALUE clearVal = { desc.Format, {0.0f, 0.0f, 0.0f, 0.0f} };

	m_hitProxyRT.Reset();
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearVal, IID_PPV_ARGS(&m_hitProxyRT)));
	device->CreateRenderTargetView(m_hitProxyRT.Get(), nullptr, allocator->GetRTVCpu(m_hitProxyRTV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
		.Format = desc.Format,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = {
			.MipLevels = 1
		}
	};
	device->CreateShaderResourceView(m_hitProxyRT.Get(), &srvDesc, allocator->GetStaticCpu(m_hitProxySRV));
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderSystem::GetHitProxySRV() const
{
	return EngineCore::GetDescriptorAllocator()->GetStaticGpu(m_hitProxySRV);
}

uint64_t RenderSystem::RequestPicking(uint32_t x, uint32_t y)
{
	m_bPickingRequested = true; 
	m_pickX = x; 
	m_pickY = y;
	return EngineCore::GetNextFenceValue();
}

DirectX::XMUINT4 RenderSystem::GetHitProxyPixel()
{
	XMUINT4 result{ 0,0,0,0 };
	if (!m_hitProxyRT || !m_hitProxyReadback) return result;

	uint8_t* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, 4 };
	if (SUCCEEDED(m_hitProxyReadback->Map(0, &readRange, reinterpret_cast<void**>(&mappedData))))
	{
		result.x = mappedData[0]; // R
		result.y = mappedData[1]; // G
		result.z = mappedData[2]; // B
		result.w = mappedData[3]; // A
		m_hitProxyReadback->Unmap(0, nullptr);
	}
	return result;
}

RenderItem* RenderSystem::AllocateRenderItem()
{
	if (m_allocatedItemCount >= m_renderItems.size())
	{
		Log::Print(ELogVerbosity::Fatal, "RenderSystem", "RenderItem Capacity Exceeded");
		return nullptr;
	}
	RenderItem* item = &m_renderItems[m_allocatedItemCount++];
	item->Reset();
	return item;
}

bool RenderSystem::SubmitToQueue(std::string_view psoName, const RenderItem* item)
{
	if (item->instanceCount == 0 || item->indexCount == 0 || !item->meshBuffer) return false;

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

uint64_t RenderSystem::GenerateSortKey(uint16_t rsIndex, uint16_t psoIndex, const RenderItem* item)
{
	uint64_t key = 0;

	// TODO : 투명 오브젝트들의 Key 별도 처리
	//if (bTransparent)
	//{ ... }

	// RS -> PSO -> Material 정렬
	key |= ((uint64_t)rsIndex << 48);
	key |= ((uint64_t)psoIndex << 32);
	key |= ((uint64_t)item->materialIndex);
	return key;
}

void RenderSystem::ExecuteQueue(ID3D12GraphicsCommandList* cmd, bool bIDPass)
{
	uint16_t currentRSIndex = 0xFFFF;
	uint16_t currentPSOIndex = 0xFFFF;

	for (const auto& entry : m_renderQueue)
	{
		uint16_t nextPSOIndex = entry.psoIndex;

		// ID Pass일 경우 mapping된 pso 교체
		if (bIDPass)
		{
			auto it = m_psoToIDPsoMap.find(entry.psoIndex);
			if (it == m_psoToIDPsoMap.end()) continue;

			nextPSOIndex = it->second;
		}

		if (currentPSOIndex != nextPSOIndex)
		{
			auto pipelineData = m_psoList->Get(nextPSOIndex);
			uint16_t nextRSIndex = m_psoList->GetRSIndex(nextPSOIndex);

			if (currentRSIndex != nextRSIndex)
			{
				cmd->SetGraphicsRootSignature(pipelineData.rs);
				currentRSIndex = nextRSIndex;

				// 카메라 버퍼
				cmd->SetGraphicsRootConstantBufferView(m_psoList->GetRootParameterIndex(currentRSIndex, "CameraBuffer"), m_cameraBuf.gpuVA);
				// 라이팅 버퍼
				cmd->SetGraphicsRootConstantBufferView(m_psoList->GetRootParameterIndex(currentRSIndex, "LightBuffer"), m_lightsBuf.gpuVA);
				// Material Table
				cmd->SetGraphicsRootDescriptorTable(m_psoList->GetRootParameterIndex(currentRSIndex, "MaterialTable"), m_materialRegistry->GetGpuDescriptorHandle());
				//Textrue Table
				cmd->SetGraphicsRootDescriptorTable(m_psoList->GetRootParameterIndex(currentRSIndex, "TextureTable"), m_textureRegistry->GetGpuDescriptorHandle(0));//Table의 시작 지점(0번 텍스쳐 위치)을 넘겨준다
			}
			cmd->SetPipelineState(pipelineData.pso);
			currentPSOIndex = nextPSOIndex;
		}

		if (bIDPass)
		{
			// IDPass일 경우 ObjectID를 추가 바인딩
			cmd->SetGraphicsRoot32BitConstant(m_psoList->GetRootParameterIndex(currentRSIndex, "ObjectID"), entry.item->objectID, 0);
		}

		for (const auto& binding : entry.item->resourceBindings)
		{
			UINT rootParamIndex = m_psoList->GetRootParameterIndex(currentRSIndex, binding.rootParamKey);
			if (rootParamIndex == UINT32_MAX)
			{
				Log::Print(ELogVerbosity::Fatal, "ExecuteQueue", "Invalid ResourceBinding : {}", binding.rootParamKey);
				continue; // 잘못된 바인딩요청일 경우 무시
			}

			switch (binding.type)
			{
				case EBindingType::CONSTANTS:
					cmd->SetGraphicsRoot32BitConstant(rootParamIndex, binding.constantData, 0);
					break;
				case EBindingType::CBV:
					cmd->SetGraphicsRootConstantBufferView(rootParamIndex, binding.gpuAddress);
					break;
				case EBindingType::SRV:
					cmd->SetGraphicsRootShaderResourceView(rootParamIndex, binding.gpuAddress);
					break;
				case EBindingType::UAV:
					cmd->SetGraphicsRootUnorderedAccessView(rootParamIndex, binding.gpuAddress);
					break;
				case EBindingType::TABLE:
					cmd->SetGraphicsRootDescriptorTable(rootParamIndex, binding.gpuDescriptorHandle);
					break;
				default:
					Log::Print(ELogVerbosity::Fatal, "RenderItem", "Invalid Binding Type!!!!\n Check For: {}", entry.item->debugName);
					break;
			}
		}

		DrawItem(cmd, entry.item);
	}
}
