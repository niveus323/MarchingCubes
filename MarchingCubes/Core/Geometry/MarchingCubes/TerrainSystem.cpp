#include "pch.h"
#include "TerrainSystem.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/CPU/MC33/MC33TerrainBackend.h"
#include "Core/Geometry/MarchingCubes/TerrainChunkRenderer.h"

TerrainSystem::TerrainSystem(ID3D12Device* device, std::shared_ptr<SdfField<float>> grd, const GridDesc& desc, TerrainMode mode):
	m_desc(desc)
{
	setMode(device, mode);
	initializeField(device, grd, desc);
}

void TerrainSystem::setMode(ID3D12Device* device, TerrainMode mode)
{
	m_mode = mode;

	switch (m_mode)
	{
		case TerrainMode::GPU_ORIGINAL:
		{
			m_backend = std::make_unique<GPUTerrainBackend>(device, m_desc);
		}
		break;
		case TerrainMode::CPU_MC33:
		default:
		{
			m_backend = std::make_unique<MC33TerrainBackend>(device, m_desc);
		}
		break;
	}

}

void TerrainSystem::setGridDesc(ID3D12Device* device, const GridDesc& d)
{
	m_desc = d;
	m_backend->setGridDesc(d);
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

void TerrainSystem::MakeDebugCell(MeshData& outMeshData)
{
	outMeshData.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

	const int Nx = static_cast<int>(m_desc.cells.x);
	const int Ny = static_cast<int>(m_desc.cells.y);
	const int Nz = static_cast<int>(m_desc.cells.z);

	// XY-Plane
	for (int x = 0; x < Nx; ++x)
	{
		for (int y = 0; y < Ny; ++y)
		{
			uint32_t index = outMeshData.indices.size();
			Vertex A{};
			A.pos = { 
				m_desc.origin.x + x * m_desc.cellsize, 
				m_desc.origin.y + y * m_desc.cellsize,
				m_desc.origin.z 
			};
			A.normal = { 0.0f, 0.0f, 1.0f };
			A.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			Vertex B{};
			B.pos = { 
				A.pos.x, 
				A.pos.y, 
				A.pos.z + m_desc.cells.z * m_desc.cellsize 
			};
			B.normal = { 0.0f, 0.0f, 1.0f };
			B.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
		
	}

	// XZ-Plane
	for (int x = 0; x < Nx; ++x)
	{
		for (int z = 0; z < Nz; ++z)
		{
			uint32_t index = outMeshData.indices.size();
			Vertex A{};
			A.pos = {
				m_desc.origin.x + x * m_desc.cellsize,
				m_desc.origin.y,
				m_desc.origin.z + z * m_desc.cellsize
			};
			A.normal = { 0.0f, 1.0f, 0.0f };
			A.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			Vertex B{};
			B.pos = {
				A.pos.x,
				A.pos.y + m_desc.cells.y * m_desc.cellsize ,
				A.pos.z 
			};
			B.normal = { 0.0f, 0.0f, 0.0f };
			B.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
	}

	// YZ-Plane
	for (int y = 0; y < Ny; ++y)
	{
		for (int z = 0; z < Nz; ++z)
		{
			uint32_t index = outMeshData.indices.size();
			Vertex A{};
			A.pos = {
				m_desc.origin.x,
				m_desc.origin.y + y * m_desc.cellsize,
				m_desc.origin.z + z * m_desc.cellsize
			};
			A.normal = { 0.0f, 0.0f, 0.0f };
			A.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			Vertex B{};
			B.pos = {
				A.pos.x + m_desc.cells.x * m_desc.cellsize ,
				A.pos.y,
				A.pos.z
			};
			B.normal = { 0.0f, 0.0f, 0.0f };
			B.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
	}
}

void TerrainSystem::initializeField(ID3D12Device* device, std::shared_ptr<SdfField<float>> grid, const GridDesc& desc)
{
	m_lastGRD = std::move(grid);
	if (m_chunkRenderer)
	{
		m_chunkRenderer.reset();
	}

	m_chunkRenderer = std::make_unique<TerrainChunkRenderer>(device);
	if (m_backend && m_lastGRD) m_backend->setFieldPtr(m_lastGRD);
}

