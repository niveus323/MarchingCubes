#include "pch.h"
#include "TerrainSystem.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/CPU/MC33/MC33TerrainBackend.h"
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"
#include "Core/Engine/EngineCore.h"

TerrainSystem::~TerrainSystem() = default;

void TerrainSystem::Initialize()
{
	if (m_chunkRenderer)
	{
		m_chunkRenderer.reset();
	}
	m_chunkRenderer = std::make_unique<MeshChunkRenderer>();
}

void TerrainSystem::Update(float deltaTime)
{
	if (m_backend->HasRequests())
	{
		ExecuteCompute(EngineCore::GetFrameIndex());
	}

	tryFetch();
}

void TerrainSystem::ExecuteCompute(uint32_t frameIndex)
{
	if (!m_backend) return;

	switch(m_mode)
	{
		case TerrainMode::GPU_ORIGINAL:
		{
			auto* uploadContext = EngineCore::GetUploadContext();
			auto* descriptorAllocator = EngineCore::GetDescriptorAllocator();
			if (auto* gpuBackend = dynamic_cast<GPUTerrainBackend*>(m_backend.get()))
			{
				gpuBackend->ExecuteCompute(frameIndex, uploadContext, descriptorAllocator);
			}
		}
		break;
		default:
		break;
	}
}

void TerrainSystem::LoadTerrain(TerrainMode mode, const GridDesc& desc, std::shared_ptr<SdfField<float>> field, const float isoValue)
{
	m_desc = desc;
	m_lastGRD = field;
	m_mode = mode; 
	RebuildBackend();
	RequestRemesh();
}

void TerrainSystem::SetMapData(const GridDesc& desc, std::shared_ptr<SdfField<float>> field)
{
	m_desc = desc;
	m_lastGRD = field;

	if (m_backend)
	{
		m_backend->setGridDesc(m_desc);
		m_backend->setFieldPtr(m_lastGRD);
	}
}

void TerrainSystem::SetMode(TerrainMode mode)
{
	if (m_desc.cellsize <= 0.0f || m_desc.cells.x == 0)
	{
		Log::Print("TerrainSystem", "Map data가 설정되어 있지 않아 로드할 수 없습니다.");
		return;
	}

	if (m_mode != mode || !m_backend)
	{
		m_mode = mode;
		RebuildBackend();
	}
}

void TerrainSystem::SetGridDesc(const GridDesc& d)
{
	m_desc = d;
	if(m_backend) m_backend->setGridDesc(d);
}

void TerrainSystem::SetField(std::shared_ptr<SdfField<float>> field)
{
	m_lastGRD = std::move(field);
	if (m_backend && m_lastGRD) m_backend->setFieldPtr(m_lastGRD);
}

void TerrainSystem::RequestRemesh(const std::set<ChunkKey>& chunkSet)
{
	if (!m_backend) return;

	m_backend->RequestRemesh(chunkSet);
}

// 전체 Remesh
void TerrainSystem::RequestRemesh()
{
	if (!m_backend) return;

	std::set<ChunkKey> chunkSet;
	uint32_t chunkX = m_desc.cells.x / m_desc.chunkSize;
	uint32_t chunkY = m_desc.cells.y / m_desc.chunkSize;
	uint32_t chunkZ = m_desc.cells.z / m_desc.chunkSize;
	for (uint32_t x = 0; x < chunkX; ++x)
		for (uint32_t y = 0; y < chunkY; ++y)
			for (uint32_t z = 0; z < chunkZ; ++z)
				chunkSet.insert(ChunkKey{ x,y,z });
	RequestRemesh(chunkSet);
}

void TerrainSystem::RequestBrush(const BrushRequest& r)
{
	if (!m_backend) return;

	m_backend->RequestBrush(r);
}

void TerrainSystem::tryFetch()
{
	if (!m_backend) return;

	std::vector<ChunkUpdate> ups;
	if (m_backend && m_backend->tryFetch(ups))
	{
		auto* uploadContext = EngineCore::GetUploadContext();
		m_chunkRenderer->ApplyUpdates(uploadContext, ups);
	}
}

void TerrainSystem::ResetRenderer() 
{
	m_chunkRenderer->Clear(); 
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
#endif

void TerrainSystem::RebuildBackend()
{
	m_backend.reset();

	auto* device = EngineCore::GetDevice();
	switch (m_mode)
	{
		case TerrainMode::GPU_ORIGINAL:
		{
			auto* descriptorAllocator = EngineCore::GetDescriptorAllocator();
			m_backend = std::make_unique<GPUTerrainBackend>(device, m_desc, descriptorAllocator);
		}
		break;
		case TerrainMode::CPU_MC33:
		default:
		{
			m_backend = std::make_unique<MC33TerrainBackend>(device, m_desc);
		}
		break;
	}

	if (m_lastGRD)
	{
		m_backend->setFieldPtr(m_lastGRD);
	}
}