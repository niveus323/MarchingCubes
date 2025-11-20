#include "pch.h"
#include "TerrainSystem.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/CPU/MC33/MC33TerrainBackend.h"
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"
#include "Core/Rendering/RenderSystem.h"

TerrainSystem::TerrainSystem(ID3D12Device* device, std::shared_ptr<SdfField<float>> grd, const GridDesc& desc, TerrainMode mode):
	m_desc(desc)
{
	if (m_chunkRenderer)
	{
		m_chunkRenderer.reset();
	}
	m_chunkRenderer = std::make_unique<MeshChunkRenderer>();
	setMode(device, mode);
	setField(device, grd);
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

void TerrainSystem::setField(ID3D12Device* device, std::shared_ptr<SdfField<float>> grid)
{
	m_lastGRD = std::move(grid);
	if (m_backend && m_lastGRD) m_backend->setFieldPtr(m_lastGRD);
}

void TerrainSystem::requestRemesh(const RemeshRequest& r)
{
	m_backend->requestRemesh(r);
}

void TerrainSystem::requestRemesh(float isoValue)
{
	RemeshRequest req{ .isoValue = isoValue };
	uint32_t chunkX = m_desc.cells.x / m_desc.chunkSize;
	uint32_t chunkY = m_desc.cells.y / m_desc.chunkSize;
	uint32_t chunkZ = m_desc.cells.z / m_desc.chunkSize;
	for (uint32_t x = 0; x < chunkX; ++x)
		for (uint32_t y = 0; y < chunkY; ++y)
			for (uint32_t z = 0; z < chunkZ; ++z)
				req.chunkset.insert(ChunkKey{ x,y,z });
	requestRemesh(req);
}

void TerrainSystem::requestBrush(const BrushRequest& r)
{
	if (!m_backend) return;
	m_backend->requestBrush(r);
}

void TerrainSystem::tryFetch(ID3D12Device* device, RenderSystem* renderSystem, const std::string& psoName)
{
	std::vector<ChunkUpdate> ups;
	if (m_backend && m_backend->tryFetch(ups))
	{
		m_chunkRenderer->ApplyUpdates(device, ups);

		auto pd = m_chunkRenderer->GetPendingDeletes();
		for (auto& drawablePtr : pd)
		{
			renderSystem->UnRegisterDynamic(drawablePtr.get(), psoName);
		}

		auto chunks = m_chunkRenderer->GetChunkDrawables();
		for (auto& drawable : chunks)
		{
			// 이미 등록되어 있으면 update
			if (renderSystem->IsDynamicRegistered(drawable, psoName)) renderSystem->UpdateDynamic(drawable, *drawable->GetCPUData());
			else renderSystem->RegisterDynamic(drawable, psoName);
		}
	}
}

#ifdef _DEBUG
void TerrainSystem::MakeDebugCell(GeometryData& outMeshData, bool bDrawFullCell)
{
	outMeshData.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

	const int Nx = static_cast<int>(m_desc.cells.x);
	const int Ny = static_cast<int>(m_desc.cells.y);
	const int Nz = static_cast<int>(m_desc.cells.z);

	// XY-Plane
	for (int x = 0; x < Nx; ++x)
	{
		if (!bDrawFullCell && (x > 0 && x < Nx - 1))
		{
			x = Nx - 2;
			continue;
		}
		for (int y = 0; y < Ny; ++y)
		{
			if (!bDrawFullCell && (y > 0 && y < Ny - 1))
			{
				y = Ny - 2;
				continue;
			}

			uint32_t index = static_cast<uint32_t>(outMeshData.indices.size());
			Vertex A{
				.pos = { m_desc.origin.x + x * m_desc.cellsize, m_desc.origin.y + y * m_desc.cellsize, m_desc.origin.z },
				.normal = { 0.0f, 0.0f, 1.0f },
				.color = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			
			Vertex B{
				.pos = { A.pos.x, A.pos.y, A.pos.z + m_desc.cells.z * m_desc.cellsize },
				.normal = { 0.0f, 0.0f, 1.0f },
				.color = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			
			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
		
	}

	// XZ-Plane
	for (int x = 0; x < Nx; ++x)
	{
		if (!bDrawFullCell && (x > 0 && x < Nx - 1))
		{
			x = Nx - 2;
			continue;
		}

		for (int z = 0; z < Nz; ++z)
		{
			if (!bDrawFullCell && (z > 0 && z < Nz - 1))
			{
				z = Nz - 2;
				continue;
			}

			uint32_t index = static_cast<uint32_t>(outMeshData.indices.size());
			Vertex A{
				.pos = { m_desc.origin.x + x * m_desc.cellsize, m_desc.origin.y, m_desc.origin.z + z * m_desc.cellsize },
				.normal = { 0.0f, 1.0f, 0.0f },
				.color = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			
			Vertex B{
				.pos = { A.pos.x, A.pos.y + m_desc.cells.y * m_desc.cellsize , A.pos.z },
				.normal = { 0.0f, 0.0f, 0.0f },
				.color = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			
			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
	}

	// YZ-Plane
	for (int y = 0; y < Ny; ++y)
	{
		if (!bDrawFullCell && (y > 0 && y < Ny - 1))
		{
			y = Ny - 2;
			continue;
		}

		for (int z = 0; z < Nz; ++z)
		{
			if (!bDrawFullCell && (z > 0 && z < Nz - 1))
			{
				z = Nz - 2;
				continue;
			}

			uint32_t index = static_cast<uint32_t>(outMeshData.indices.size());
			Vertex A{
				.pos = { m_desc.origin.x, m_desc.origin.y + y * m_desc.cellsize, m_desc.origin.z + z * m_desc.cellsize },
				.normal = { 0.0f, 0.0f, 0.0f },
				.color = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			
			Vertex B{
				.pos = { A.pos.x + m_desc.cells.x * m_desc.cellsize , A.pos.y, A.pos.z },
				.normal = { 0.0f, 0.0f, 0.0f },
				.color = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			
			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
	}
}

void TerrainSystem::EraseChunk(RenderSystem* renderSystem)
{
	m_chunkRenderer->Clear();
	auto pd = m_chunkRenderer->GetPendingDeletes();
	for (auto& drawablePtr : pd)
	{
		renderSystem->UnRegisterDynamic(drawablePtr.get(), "Filled");
	}
}
#endif