#include "pch.h"
#include "GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesCS.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Rendering/UploadContext.h"

/*
* 지형 편집은 CPU에서 진행하고 GPU에서는 '받은 데이터를 읽기만 해서 메쉬 버퍼를 쓰기만 한다'는 규칙을 지킨다.
* Flow 예시 : A요청에 대해 MarchingCubesCS 실행->Readback 버퍼에 복사 -> Signal -> B요청에 대해 MarchingCubesCS 연산 -> Readback 버퍼에 복사 -> Signal
*				CPU는 매 프레임 시작 시 
*/

static inline ChunkKey DecodeChunkKey(uint32_t idx, const XMUINT3& resolution)
{
	const uint32_t nx = resolution.x, ny = resolution.y, nxy = nx * ny;
	ChunkKey k;
	k.z = idx / nxy;
	idx -= k.z * nxy;
	k.y = idx / nx;
	k.x = idx - k.y * nx;
	return k;
}

GPUTerrainBackend::GPUTerrainBackend(ID3D12Device* device, DescriptorAllocator* descriptorAllocator) :
	m_device(device),
	m_descriptorAllocator(descriptorAllocator),
	m_fenceEvent(nullptr)
{
	m_mc = std::make_unique<GPUMarchingCubesCS>(device);

	D3D12_COMMAND_QUEUE_DESC queueDesc{
		.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	};
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_commandQueue);

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(m_commandAllocator.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_commandAllocator);

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_fence);
	m_fenceValues = 0;

	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
	ThrowIfFailed(m_commandList->Close());

	if (m_descriptorAllocator) m_triTableSlot = m_descriptorAllocator->AllocateStaticSlot();
}

GPUTerrainBackend::~GPUTerrainBackend()
{
	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
	}
}

//텍스처 슬롯, 결과 담을 UAV 버퍼, Readback 버퍼 생성
void GPUTerrainBackend::Initialize(uint32_t poolSize, uint32_t maxChunkSize)
{
	m_maxChunkSize = maxChunkSize;
	m_chunkSlots.resize(poolSize);

	uint32_t maxTexSize = m_maxChunkSize + 2; // 양옆 Padding 1칸씩
	m_tempExtractBuffer.resize(maxTexSize * maxTexSize * maxTexSize);

	for (uint32_t i = 0; i < poolSize; ++i)
	{
		CreateSlotResource(i);
	}

	uint64_t elementSize = sizeof(OutTriangle);
	uint64_t bufferSize = static_cast<uint64_t>(MAX_TOTAL_TRIANGLES) * elementSize;

	// Default Heap (GPU 전용)
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
		IID_PPV_ARGS(&m_outBuffer)
	));
	NAME_D3D12_OBJECT_ALIAS(m_outBuffer, L"MC_TraingleOutput");

	// Counter Buffer (AppendBuffer용)
	auto counterDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &counterDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
		IID_PPV_ARGS(&m_outCounter)
	));
	NAME_D3D12_OBJECT_ALIAS(m_outCounter, L"MC_Counter");

	// Readback 버퍼 생성
	auto rbHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	auto rbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize + sizeof(uint32_t)); // 데이터(Size) + 카운터(4byte)
	ThrowIfFailed(m_device->CreateCommittedResource(
		&rbHeapProp, D3D12_HEAP_FLAG_NONE, &rbDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&m_readbackBuffer)
	));
	m_readbackBuffer->SetName(L"MC_Readback");

	// UAV Descriptor 생성
	m_outBufferUavIndex = m_descriptorAllocator->AllocateStaticSlot();
	DescriptorAllocator::CreateUAV_Structured(m_device, m_outBuffer.Get(), sizeof(OutTriangle), m_descriptorAllocator->GetStaticCpu(m_outBufferUavIndex), m_outCounter.Get());

}

//들어온 요청을 단순히 대기열에 넣고 끝냄 (Non-blocking)
void GPUTerrainBackend::PushRequest(BuildRequest&& request)
{
	m_waitingQueue.push_back({ std::move(request) });
}

//Readback 버퍼의 작업이 완료(Fence)되었는지 확인 -> 버퍼 Map -> ChunkID 별로 결과 분류(Sorting) -> 최종 결과 반환
bool GPUTerrainBackend::TryFetch(std::vector<BuildResult>& OutResults)
{
	if (m_fence->GetCompletedValue() < m_lastReadbackFence || m_inFlightJobs.empty()) return false;

	uint8_t* pData = nullptr;
	D3D12_RANGE readRange = { 0, m_readbackBuffer->GetDesc().Width };
	ThrowIfFailed(m_readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(pData)));

	uint32_t count = *reinterpret_cast<uint32_t*>(pData);
	count = std::min(count, MAX_TOTAL_TRIANGLES);

	// 내용물 포장
	OutTriangle* triangles = reinterpret_cast<OutTriangle*>(pData + sizeof(uint32_t));
	std::unordered_map<uint32_t, BuildResult> resultMap;
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& tri = triangles[i];
		BuildResult& res = resultMap[tri.chunkIdx];
		uint32_t baseIdx = static_cast<uint32_t>(res.vertices.size());
		
		res.vertices.push_back(tri.A);
		res.vertices.push_back(tri.B);
		res.vertices.push_back(tri.C);

		res.indices.push_back(baseIdx + 0);
		res.indices.push_back(baseIdx + 1);
		res.indices.push_back(baseIdx + 2);
	}

	// 발송지 표기
	for (const auto& job : m_inFlightJobs)
	{
		BuildResult& res = resultMap[job.slotIndex];
		res.key = job.request.key;
		res.ptr = job.request.ptr;
		OutResults.push_back(std::move(res));
	}

	m_readbackBuffer->Unmap(0, nullptr);
	m_inFlightJobs.clear();

	return true;
}

//빈 슬롯에 대기열의 작업을 할당 -> 데이터 업로드 -> Compute Shader 실행 -> 결과를 Readback 버퍼로 복사 명령
void GPUTerrainBackend::ExecuteCompute(uint32_t frameIndex, UploadContext* uploadContext, DescriptorAllocator* descriptorAllocator)
{
    uint64_t completedFence = m_fence->GetCompletedValue();
    for (auto& slot : m_chunkSlots)
    {
        if (!slot.isFree && slot.completionFence <= completedFence)
        {
            slot.isFree = true;
        }
    }
    if (m_waitingQueue.empty()) return;

    ID3D12GraphicsCommandList* cmd = m_commandList.Get();
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

	ID3D12DescriptorHeap* heaps[] = { m_descriptorAllocator->GetCbvSrvUavHeap() };
	m_commandList->SetDescriptorHeaps(1, heaps);

    bool workSubmitted = false;
	while (!m_waitingQueue.empty())
	{
		ChunkSlot* slot = nullptr;
		for (auto& s : m_chunkSlots)
		{
			if (s.isFree)
			{
				slot = &s;
				break;
			}
		}
		if (!slot) break; // 슬롯이 없으면 이번 프레임 처리는 여기서 중단 (다음 프레임에 계속)

		PendingRequest req = std::move(m_waitingQueue.front());
		m_waitingQueue.pop_front();
		slot->isFree = false;

		ExtractChunkData(req.request.fieldData.get(), req.request.key, req.request.setting, m_tempExtractBuffer);

		uint32_t cellsPerChunk = req.request.setting.cellsPerChunk;
		uint32_t borderedChunkSize = req.request.setting.cellsPerChunk + 2;
		D3D12_SUBRESOURCE_DATA subData{
			.pData = m_tempExtractBuffer.data(),
			.RowPitch = borderedChunkSize * sizeof(float),
			.SlicePitch = borderedChunkSize * borderedChunkSize * sizeof(float)
		};
		uploadContext->UploadTexture(m_commandList.Get(), slot->volumeTexture.Get(), { subData }, 
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, "ChunkUpload");

		GridCBData cbData{
			.gridCells = req.request.setting.resolution,
			.gridOrigin = req.request.setting.origin,
			.isoValue = req.request.setting.isoValue,
			.numChunkAxis = { 0,0,0 },
			.chunkCubes = cellsPerChunk,
			.regionCellMin = { 0,0,0 },
			.regionCellMax = { cellsPerChunk, cellsPerChunk, cellsPerChunk }
		};
		BufferHandle cbHandle;
		uploadContext->UploadConstants(&cbData, sizeof(GridCBData), cbHandle);

		const uint32_t alignedMaxChunkSize = m_maxChunkSize + 2;
        m_mc->encode(GPUMCEncodingContext{
			.device = m_device,
			.cmd = m_commandList.Get(),
			.chunkCubes = cellsPerChunk,
			.gridDimension = { alignedMaxChunkSize, alignedMaxChunkSize, alignedMaxChunkSize },
			.regionCellMin = { 0, 0, 0 },
			.regionCellMax = { cellsPerChunk, cellsPerChunk, cellsPerChunk },
			.cbAddress = cbHandle.gpuVA,
			.triTableSrv = descriptorAllocator->GetStaticGpu(0),
			.densitySrv = descriptorAllocator->GetStaticGpu(slot->srvIndex),
			.outBufferUav = descriptorAllocator->GetDynamicGpu(frameIndex, m_outBufferUavIndex)
		});

        m_inFlightJobs.push_back(InFlightJob{
			.slotIndex = slot->index,
			.request = std::move(req.request)
		});
        workSubmitted = true;
    }

    if (workSubmitted)
    {
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		m_commandList->ResourceBarrier(1, &barrier);

		// 카운터 복사
		m_commandList->CopyBufferRegion(m_readbackBuffer.Get(), 0, m_outCounter.Get(), 0, sizeof(uint32_t));

		// 데이터 복사
		m_commandList->CopyBufferRegion(m_readbackBuffer.Get(), sizeof(uint32_t), m_outBuffer.Get(), 0, m_outBuffer->GetDesc().Width);

		CD3DX12_RESOURCE_BARRIER backBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_commandList->ResourceBarrier(1, &backBarrier);

		uploadContext->ResetCounterUAV(m_commandList.Get(), m_outCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// 제출
		m_fenceValues++;
		m_commandList->Close();
		ID3D12CommandList* cmds[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(1, cmds);
		m_commandQueue->Signal(m_fence.Get(), m_fenceValues);
		m_lastReadbackFence = m_fenceValues;

		for (auto& job : m_inFlightJobs)
		{
			if (m_chunkSlots[job.slotIndex].completionFence < m_fenceValues)
			{
				m_chunkSlots[job.slotIndex].completionFence = m_fenceValues;
			}
		}
    }
	else
	{
		//Reset호출 시점에 CommandList가 닫혀있는지 아닌지를 구분할 수 없으므로 작성할 일이 없다면 일단 닫아둔다
		m_commandList->Close();
	}
}

void GPUTerrainBackend::InitializeSlots(ID3D12Device* device, int count, int slotSize)
{
	m_chunkSlots.resize(count);
	for (int i = 0; i < count; ++i)
	{
		uint64_t alignedSize = AlignUp64(slotSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_chunkSlots[i].volumeTexture)));
		NAME_D3D12_OBJECT_ALIAS(m_chunkSlots[i].volumeTexture, L"TerrainSystem TexrturePool");
	}
}

void GPUTerrainBackend::CreateSlotResource(int index)
{
	auto& slot = m_chunkSlots[index];
	slot.index = index;
	slot.isFree = true;
	slot.completionFence = 0;

	uint32_t texSize = m_maxChunkSize + 2; // Border 양옆 1씩 적용
	D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D,
		.Width = texSize,
		.Height = texSize,
		.DepthOrArraySize = static_cast<UINT16>(texSize),
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R32_FLOAT,
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
	};
	CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(m_device->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(slot.volumeTexture.ReleaseAndGetAddressOf())
	));
	NAME_D3D12_OBJECT_ALIAS(slot.volumeTexture, std::format(L"ChunkSlot_{}", index).c_str());

	// SRV 생성
	slot.srvIndex = m_descriptorAllocator->AllocateStaticSlot();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
		.Format = desc.Format,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture3D = D3D12_TEX3D_SRV{
			.MostDetailedMip = 0,
			.MipLevels = 1
		}
	};
	m_device->CreateShaderResourceView(slot.volumeTexture.Get(), &srvDesc, m_descriptorAllocator->GetStaticCpu(slot.srvIndex));
}

void GPUTerrainBackend::ExtractChunkData(const SdfField* field, const ChunkKey& key, const GridDesc& desc, std::vector<float>& outBuffer)
{
	const int cellsPerChunk = desc.cellsPerChunk;
	const int border = 1;
	const int extractSize = cellsPerChunk + (border * 2);
	const int totalVoxels = extractSize * extractSize * extractSize;

	const int sx = field->sx();
	const int sy = field->sy();
	const int sz = field->sz();

	const int baseX = key.x * cellsPerChunk;
	const int baseY = key.y * cellsPerChunk;
	const int baseZ = key.z * cellsPerChunk;

	int outIndex = 0;
	for (int z = -border; z < cellsPerChunk + border; ++z)
	{
		int gz = std::clamp(baseZ + z, 0, sz - 1);

		for (int y = -border; y < cellsPerChunk + border; ++y)
		{
			int gy = std::clamp(baseY + y, 0, sy - 1);

			for (int x = -border; x < cellsPerChunk + border; ++x)
			{
				int gx = std::clamp(baseX + x, 0, sx - 1);
				outBuffer[outIndex++] = field->at(gx,gy,gz);
			}
		}
	}
}

