#include "pch.h"
#include "GPUTerrainBackend.h"
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"
#include "Core/Geometry/MarchingCubes/GPU/SDFVolume3D.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUBrushCS.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUMarchingCubesCS.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Rendering/UploadContext.h"

static inline ChunkKey DecodeChunkKey(uint32_t idx, const XMUINT3& cells)
{
	const uint32_t nx = cells.x, ny = cells.y, nxy = nx * ny;
	ChunkKey k;
	k.z = idx / nxy;
	idx -= k.z * nxy;
	k.y = idx / nx;
	k.x = idx - k.y * nx;
	return k;
}

GPUTerrainBackend::GPUTerrainBackend(ID3D12Device* device, const GridDesc& gridDesc, const GPUTerrainInitInfo& init) :
	m_device(device),
	m_grid(gridDesc),
	m_descriptorAllocator(init.descriptorAllocator),
	m_uploadContext(init.uplaodContext),
	m_fenceEvent(nullptr)
{
	m_vol = std::make_unique<SDFVolume3D>(device, m_uploadContext);
	m_brush = std::make_unique<GPUBrushCS>(device);
	m_mc = std::make_unique<GPUMarchingCubesCS>(device);

	D3D12_COMMAND_QUEUE_DESC queueDesc{
		.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	};
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_commandQueue);

	for (uint32_t n = 0; n < kRBFrameCount; ++n)
	{
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(m_commandAllocator[n].ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT_INDEXED(m_commandAllocator, n);
	}

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_fence);
	m_fenceValues = 1;
	m_lastSubmitFenceValues = 0;

	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_commandAllocator[m_rbCursor].Get(), nullptr, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
	ThrowIfFailed(m_commandList->Close());

	computeNumChunks();
	ensureTriangleBuffer();
	for (uint32_t i = 0; i < kRBFrameCount; ++i)
	{
		ensureRBSlot(i);
	}

	if (m_descriptorAllocator) m_triTableSlot = m_descriptorAllocator->AllocateStaticSlot();
}

GPUTerrainBackend::~GPUTerrainBackend()
{
	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
	}
}

void GPUTerrainBackend::setGridDesc(const GridDesc& desc)
{
	m_grid = desc;

	computeNumChunks();
	// readback 버퍼 재생성
	m_outBuffer.Reset();
	ensureTriangleBuffer();
	for (uint32_t i = 0; i < kRBFrameCount; ++i)
	{
		resetRBSlot(i);
		ensureRBSlot(i);
	}
}

void GPUTerrainBackend::setFieldPtr(std::shared_ptr<SdfField<float>> grid)
{
	m_gridData = std::move(grid);
	//SDFVolume3D의 Upload는 m_fieldDirty를 체크하여 encode에서 처리.
	m_fieldDirty = true;
}

void GPUTerrainBackend::requestBrush(uint32_t frameIndex, const BrushRequest& r)
{
	m_requestedBrush = r;
	m_hasBrush = true;

	// Brush를 처리한다는건 메쉬 갱신이 필요하다는 뜻
	requestRemesh(frameIndex, RemeshRequest(m_requestedRemesh.isoValue));
}

// 전체 Chunk 리빌드 요청
void GPUTerrainBackend::requestRemesh(uint32_t frameIndex, const RemeshRequest& r)
{
	m_requestedRemesh = r;
	m_needsRemesh = true;

	encode(frameIndex);
}

void GPUTerrainBackend::encode(uint32_t frameIndex)
{
	// encode가 발동하는 조건 : _GRD 갱신(m_fieldDirty == true), brush 사용(m_hasBrush == true)
	if (!m_device || (!m_fieldDirty && !m_hasBrush && !m_needsRemesh)) return;

	prepareComputeEncoding();

	if (m_fieldDirty) encodeFieldUpload();

	SDFVolumeView volView = {
		.tex = m_vol->density(),
		.grid = m_grid,
		.chunkCubes = s_chunkcubes,
		.numChunkAxis = m_numChunkAxis
	};
	
	XMUINT3 regionMin = { 0,0,0 };
	XMUINT3 regionMax = m_grid.cells;
	if (m_hasBrush) encodeBrushPass(frameIndex, regionMin, regionMax, volView);
	if (m_needsRemesh) encodeRemeshPass(frameIndex, regionMin, regionMax, volView);

	finishComputeEncoding();
}

bool GPUTerrainBackend::tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates)
{
	if (!m_needsFetch) return false;
	m_needsFetch = false;

	if (m_fence->GetCompletedValue() < m_lastSubmitFenceValues)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_lastSubmitFenceValues, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
	}

	auto& r = m_rb[m_rbCursor];
	void* p = nullptr;
	CD3DX12_RANGE range(0, sizeof(uint32_t));
	r.rbCount->Map(0, &range, &p);
	memcpy(&r.count, p, sizeof(uint32_t));
	r.rbCount->Unmap(0, nullptr);

	if (r.count == 0) return false;

	m_commandAllocator[m_rbCursor]->Reset();
	m_commandList->Reset(m_commandAllocator[m_rbCursor].Get(), nullptr);
	m_commandList->CopyBufferRegion(r.rbTriangles.Get(), 0, m_outBuffer.Get(), 0, r.count * sizeof(OutTriangle));
	m_commandList->Close();
	ID3D12CommandList* cmdList[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	m_commandQueue->Signal(m_fence.Get(), m_fenceValues);
	m_fence->SetEventOnCompletion(m_fenceValues, m_fenceEvent);
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
	++m_fenceValues;

	uint32_t triCount = r.count;
	// OutTriangle
	void* pTris = nullptr;
	D3D12_RANGE rrTri{ 0, (SIZE_T)triCount * sizeof(OutTriangle) };
	r.rbTriangles->Map(0, &rrTri, &pTris);
	const OutTriangle* OutTriangles = reinterpret_cast<const OutTriangle*>(pTris);

	std::vector<uint32_t> triPerChunk(m_numChunks, 0);
	for (uint32_t i = 0; i < triCount; ++i)
	{
		++triPerChunk[OutTriangles[i].chunkIdx];
	}
	std::unordered_map<uint32_t, uint32_t> outchunkUpdatesTable;
	for (uint32_t i = 0; i < m_numChunks; ++i)
	{
		if (triPerChunk[i] == 0) continue;
		ChunkUpdate up;
		up.key = DecodeChunkKey(i, m_numChunkAxis);
		up.empty = false;
		up.md.vertices.clear();
		up.md.indices.clear();
		up.md.vertices.reserve(static_cast<size_t>(triPerChunk[i]) * 3);
		up.md.indices.reserve(static_cast<size_t>(triPerChunk[i]) * 3);
		up.md.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		OutChunkUpdates.push_back(up);
		outchunkUpdatesTable.insert_or_assign(i, static_cast<uint32_t>(OutChunkUpdates.size() - 1));
	}

	for (uint32_t i = 0; i < triCount; ++i)
	{
		const OutTriangle& tri = OutTriangles[i];
		uint32_t index = outchunkUpdatesTable[tri.chunkIdx];
		ChunkUpdate& up = OutChunkUpdates[index];

		uint32_t baseIndex = static_cast<uint32_t>(up.md.vertices.size());

		GeometryData& md = up.md;
		md.vertices.push_back(Vertex{ .pos = tri.A.position, .normal = tri.A.normal , .tangent = tri.A.tangent});
		md.vertices.push_back(Vertex{ .pos = tri.B.position, .normal = tri.B.normal , .tangent = tri.B.tangent});
		md.vertices.push_back(Vertex{ .pos = tri.C.position, .normal = tri.C.normal , .tangent = tri.C.tangent});

		md.indices.push_back(baseIndex + 0);
		md.indices.push_back(baseIndex + 1);
		md.indices.push_back(baseIndex + 2);
		baseIndex += 3;
	}
	r.rbTriangles->Unmap(0, nullptr);

	// 다음 프레임을 위한 링 advance
	m_rbCursor = (m_rbCursor + 1) % kRBFrameCount;
	return !OutChunkUpdates.empty();
}

void GPUTerrainBackend::ensureTriangleBuffer()
{
	if (m_outBuffer && m_outCounter) return;

	const uint32_t numElems = m_numChunks * m_triCapPerChunk;
	const uint32_t sizeBytes = numElems * sizeof(OutTriangle);

	auto hpDef = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	auto descUav = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(m_device->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &descUav,D3D12_RESOURCE_STATE_COMMON, nullptr,IID_PPV_ARGS(m_outBuffer.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_outBuffer);

	auto descCnt = CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(m_device->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &descCnt,D3D12_RESOURCE_STATE_COMMON, nullptr,IID_PPV_ARGS(m_outCounter.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_outCounter);
}

void GPUTerrainBackend::computeNumChunks()
{
	const XMUINT3 totalCubes(m_grid.cells.x - 1, m_grid.cells.y - 1, m_grid.cells.z - 1);

	m_numChunkAxis = XMUINT3(
		(totalCubes.x + (s_chunkcubes - 1)) / s_chunkcubes,
		(totalCubes.y + (s_chunkcubes - 1)) / s_chunkcubes,
		(totalCubes.z + (s_chunkcubes - 1)) / s_chunkcubes
	);

	m_numChunks = m_numChunkAxis.x * m_numChunkAxis.y * m_numChunkAxis.z;
}

void GPUTerrainBackend::ensureRBSlot(uint32_t slot)
{
	auto& r = m_rb[slot];
	if (r.rbTriangles && r.rbCount) return;

	CD3DX12_HEAP_PROPERTIES hp_Readback(D3D12_HEAP_TYPE_READBACK);
	if (!r.rbTriangles)
	{
		const uint64_t bytes = uint64_t(m_numChunks) * uint64_t(m_triCapPerChunk) * sizeof(OutTriangle);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
		ThrowIfFailed(m_device->CreateCommittedResource(
			&hp_Readback,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(r.rbTriangles.ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT(r.rbTriangles);
	}

	if (!r.rbCount)
	{
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(4);
		ThrowIfFailed(m_device->CreateCommittedResource(
			&hp_Readback,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(r.rbCount.ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT(r.rbCount);
	}
}

void GPUTerrainBackend::resetRBSlot(uint32_t slot)
{
	auto& r = m_rb[slot];
	r.rbCount.Reset();
	r.rbTriangles.Reset();
}

void GPUTerrainBackend::prepareComputeEncoding()
{
	ThrowIfFailed(m_commandAllocator[m_rbCursor]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator[m_rbCursor].Get(), nullptr));

	ID3D12DescriptorHeap* heaps[] = { m_descriptorAllocator->GetCbvSrvUavHeap() };
	m_commandList->SetDescriptorHeaps(1, heaps);
#if PIX_DEBUGMODE
	if (m_hasBrush && PIXGetCaptureState() > 0)
	{
		PIXBeginEvent(m_commandList.Get(), PIX_COLOR(0, 255, 0), "GPUTerrainBackend");
	}
#endif
}

void GPUTerrainBackend::finishComputeEncoding()
{
#if PIX_DEBUGMODE
	if (PIXGetCaptureState() > 0)
	{
		PIXEndEvent(m_commandList.Get());
	}
#endif
	//Wait For GPU
	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Compute 큐의 작업을 최대한 병렬로 처리하기 위해 Signal만 걸어두고 tryFetch에서 대기
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues));
	m_lastSubmitFenceValues = m_fenceValues++;
	m_needsFetch = true;
}

void GPUTerrainBackend::encodeFieldUpload()
{
	m_vol->uploadFromGRD(m_commandList.Get(), m_gridData.get());

	m_fieldDirty = false;
	m_needsRemesh = true;
}

void GPUTerrainBackend::encodeBrushPass(uint32_t frameIndex, DirectX::XMUINT3& regionMin, DirectX::XMUINT3& regionMax, SDFVolumeView& volView)
{
	const int halo = 1;
	const int r = (int)std::ceil(m_requestedBrush.radius / m_grid.cellsize);

	XMUINT3 brushCenter = computeBrushCenter(m_requestedBrush.hitpos, m_grid.origin, m_grid.cellsize);
	computeBrushRegionCells(m_grid, brushCenter, m_requestedBrush.radius, regionMin, regionMax);

	BrushCBData data{
		.brushRadius = m_requestedBrush.radius,
		.brushWeight = m_requestedBrush.weight,
		.deltaTime = m_requestedBrush.deltaTime,
		.gridCells = m_grid.cells,
		.brushCenter = brushCenter,
		.regionCellMin = regionMin,
		.regionCellMax = regionMax
	};

	BufferHandle brushCB{};
	m_uploadContext->UploadContstants(frameIndex, &data, sizeof(BrushCBData), brushCB);

	uint32_t densityUavSlot = m_descriptorAllocator->AllocateDynamic(frameIndex);
	DescriptorAllocator::CreateUAV_Texture3D(m_device, m_vol->density(), DXGI_FORMAT_R32_FLOAT, m_descriptorAllocator->GetDynamicCpu(frameIndex, densityUavSlot));
	volView.uav = m_descriptorAllocator->GetDynamicGpu(frameIndex, densityUavSlot);

	GPUBrushEncodingContext context{
		.cmd = m_commandList.Get(),
		.vol = volView,
		.regionMin = regionMin,
		.regionMax = regionMax,
		.cbAddress = brushCB.gpuVA
	};
	m_brush->encode(context);
	m_hasBrush = false;

}

void GPUTerrainBackend::encodeRemeshPass(uint32_t frameIndex, const DirectX::XMUINT3& regionMin, const DirectX::XMUINT3& regionMax, SDFVolumeView& volView)
{
	uint32_t densitySrvSlot = m_descriptorAllocator->AllocateDynamic(frameIndex);
	DescriptorAllocator::CreateSRV_Texture3D(m_device, m_vol->density(), DXGI_FORMAT_R32_FLOAT, m_descriptorAllocator->GetDynamicCpu(frameIndex, densitySrvSlot));
	volView.srv = m_descriptorAllocator->GetDynamicGpu(frameIndex, densitySrvSlot);

	uint32_t outUavSlot = m_descriptorAllocator->AllocateDynamic(frameIndex);
	DescriptorAllocator::CreateUAV_Structured(m_device, m_outBuffer.Get(), sizeof(OutTriangle), m_descriptorAllocator->GetDynamicCpu(frameIndex, outUavSlot), m_outCounter.Get());

	auto trisToUav = CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_commandList->ResourceBarrier(1, &trisToUav);

	// 카운터 리셋(0)
	m_uploadContext->ResetCounterUAV(m_commandList.Get(), m_outCounter.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	XMUINT3 expandedMin, expandedMax;
	computeChunkAlignedRegion(m_grid.cells, regionMin, regionMax, expandedMin, expandedMax);

	GridCBData data{
		.gridCells = m_grid.cells,
		.gridOrigin = m_grid.origin,
		.isoValue = m_requestedRemesh.isoValue,
		.numChunkAxis = volView.numChunkAxis,
		.chunkCubes = volView.chunkCubes,
		.regionCellMin = expandedMin,
		.regionCellMax = expandedMax
	};

	BufferHandle gridCB{};
	m_uploadContext->UploadContstants(frameIndex, &data, sizeof(GridCBData), gridCB);

	DescriptorAllocator::CreateSRV_Structured(m_device, m_mc->triTable(), sizeof(int), m_descriptorAllocator->GetStaticCpu(m_triTableSlot));

	GPUMCEncodingContext ctx{
		.device = m_device,
		.cmd = m_commandList.Get(),
		.vol = volView,
		.req = m_requestedRemesh,
		.regionCellMin = expandedMin,
		.regionCellMax = expandedMax,
		.cbAddress = gridCB.gpuVA,
		.triTableSrv = m_descriptorAllocator->GetStaticGpu(m_triTableSlot),
		.outBufferUav = m_descriptorAllocator->GetDynamicGpu(frameIndex, outUavSlot)
	};
	m_mc->encode(ctx);

	// readback 스케쥴
	const uint32_t rbSlot = m_rbCursor;
	ensureRBSlot(rbSlot);

	// Output Readback
	CD3DX12_RESOURCE_BARRIER toCopySrc[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_outCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
	};
	m_commandList->ResourceBarrier(_countof(toCopySrc), toCopySrc);

	m_commandList->CopyBufferRegion(m_rb[rbSlot].rbCount.Get(), 0, m_outCounter.Get(), 0, sizeof(uint32_t));

	CD3DX12_RESOURCE_BARRIER backToUav[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_outBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(m_outCounter.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};
	m_commandList->ResourceBarrier(_countof(backToUav), backToUav);

	m_needsRemesh = false;

}

XMUINT3 GPUTerrainBackend::computeBrushCenter(const XMFLOAT3& hitpos, const XMFLOAT3& gridorigin, const float cellsize)
{
	return {
		static_cast<uint32_t>((hitpos.x - gridorigin.x) / cellsize),
		static_cast<uint32_t>((hitpos.y - gridorigin.y) / cellsize),
		static_cast<uint32_t>((hitpos.z - gridorigin.z) / cellsize)
	};
}

void GPUTerrainBackend::computeBrushRegionCells(const GridDesc& grid, const DirectX::XMUINT3& brushCenter, const float brushRadius, DirectX::XMUINT3& outRegionMin, DirectX::XMUINT3& outRegionMax)
{
	const uint32_t halo = 1;
	const XMUINT3 gridDim = grid.cells;

	const float radiusCell = brushRadius / grid.cellsize;
	const uint32_t r = static_cast<uint32_t>(std::ceil(radiusCell));

	outRegionMin.x = MathHelper::SafeSub(brushCenter.x + halo, r);
	outRegionMin.y = MathHelper::SafeSub(brushCenter.y + halo, r);
	outRegionMin.z = MathHelper::SafeSub(brushCenter.z + halo, r);

	outRegionMax.x = std::min<uint32_t>(gridDim.x, brushCenter.x + r + halo);
	outRegionMax.y = std::min<uint32_t>(gridDim.y, brushCenter.y + r + halo);
	outRegionMax.z = std::min<uint32_t>(gridDim.z, brushCenter.z + r + halo);
}

void GPUTerrainBackend::computeChunkAlignedRegion(const XMUINT3& cells, const XMUINT3& brushRegionMin, const XMUINT3& brushRegionMax, XMUINT3& outRegionMin, XMUINT3& outRegionMax)
{
	XMUINT3 cmin{
		brushRegionMin.x / s_chunkcubes,
		brushRegionMin.y / s_chunkcubes,
		brushRegionMin.z / s_chunkcubes
	};

	XMUINT3 cmax{
		(brushRegionMax.x + s_chunkcubes - 1) / s_chunkcubes,
		(brushRegionMax.y + s_chunkcubes - 1) / s_chunkcubes,
		(brushRegionMax.z + s_chunkcubes - 1) / s_chunkcubes
	};

	outRegionMin.x = std::min(cmin.x * s_chunkcubes, cells.x);
	outRegionMin.y = std::min(cmin.y * s_chunkcubes, cells.y);
	outRegionMin.z = std::min(cmin.z * s_chunkcubes, cells.z);

	outRegionMax.x = std::min(cmax.x * s_chunkcubes, cells.x);
	outRegionMax.y = std::min(cmax.y * s_chunkcubes, cells.y);
	outRegionMax.z = std::min(cmax.z * s_chunkcubes, cells.z);
}
