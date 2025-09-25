#include "pch.h"
#include "TerrainSystem.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/CPU/MC33/MC33TerrainBackend.h"
#include "Core/Geometry/MarchingCubes/TerrainChunkRenderer.h"

TerrainSystem::TerrainSystem(ID3D12Device* device, std::shared_ptr<_GRD> grd, const GridDesc& desc, TerrainMode mode)
{
	setMode(device, mode);
	initializeField(device, grd, desc);
}

void TerrainSystem::setMode(ID3D12Device* device, TerrainMode mode)
{
	m_mode = mode;

	std::unique_ptr<ITerrainBackend> next;
	switch (m_mode)
	{
		case TerrainMode::GPU_ORIGINAL:
		{
			m_backend = std::make_unique<GPUTerrainBackend>(device, m_grid);
		}
		break;

		case TerrainMode::CPU_MC33:
		default:
		{
			m_backend = std::make_unique<MC33TerrainBackend>(m_grid, m_lastGRD);
		}
		break;
	}
	
}

void TerrainSystem::setGridDesc(ID3D12Device* device, const GridDesc& d)
{
	m_grid = d;
	m_backend->setGridDesc(d);
}

void TerrainSystem::initializeField(ID3D12Device* device, std::shared_ptr<_GRD> grid, const GridDesc& desc)
{
	m_lastGRD = std::move(grid);
	if (m_chunkRenderer)
	{
		m_chunkRenderer.reset();
	}

	m_chunkRenderer = std::make_unique<TerrainChunkRenderer>(device);
	setGridDesc(device, desc);
	if(m_backend && m_lastGRD) m_backend->setFieldPtr(m_lastGRD);
}

void TerrainSystem::initializeField(ID3D12Device* device, const _GRD& grid, const GridDesc& desc)
{
	initializeField(device, std::make_shared<_GRD>(grid), desc);
}

void TerrainSystem::requestRemesh(const RemeshRequest& r)
{
	m_backend->requestRemesh(r);
}

void TerrainSystem::requestBrush(const BrushRequest& r)
{
	if (!m_backend) return;
	m_backend->requestBrush(r);
}

void TerrainSystem::tryFetch(ID3D12Device* device, ID3D12Fence* graphicsFence, std::vector<ComPtr<ID3D12Resource>>* sink)
{
	std::vector<ChunkUpdate> ups;
	if (m_backend && m_backend->tryFetch(ups))
	{
		m_chunkRenderer->ApplyUpdates(device, graphicsFence, sink, ups);
	}
}

void TerrainSystem::UploadRendererData(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, std::vector<std::pair<UINT64, UINT64>>& outAllocations)
{
	m_chunkRenderer->UploadData(device, cmd, outAllocations);
}