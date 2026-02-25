#include "pch.h"
#include "TerrainSystem.h"
#include "Core/Geometry/MarchingCubes/GPU/GPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/CPU/CPUTerrainBackend.h"
#include "Core/Geometry/MarchingCubes/Class/TerrainObject.h"
#include "Core/Geometry/MarchingCubes/Class/SdfDataComponent.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Assets/ResourceManager.h"

BEGIN_REFLECTION(TerrainSystem, ISceneSubsystem)
END_REFLECTION()

TerrainSystem::~TerrainSystem() = default;

void TerrainSystem::Initialize()
{
	SetMode(TerrainMode::CPU_MC33);

}

void TerrainSystem::Update(float deltaTime)
{
	if (!m_backend) return;

	if (m_backend->HasRequests())
	{
		ExecuteCompute(EngineCore::GetFrameIndex());
	}

	std::vector<BuildResult> finishedJobs;
	if (m_backend->TryFetch(finishedJobs))
	{
		for (auto& result : finishedJobs)
		{
			if (auto owner = result.ptr.lock())
			{
				owner->ApplyChunkMesh(result.key, std::move(result.vertices), std::move(result.indices));
			}
		}
	}
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

void TerrainSystem::RequestChunkRemesh(std::shared_ptr<TerrainObject> requester, const std::set<ChunkKey>& chunks)
{
	if (!requester || chunks.empty() || !m_backend) return;

	auto* dataComp = requester->GetComponent<SdfDataComponent>();
	if (!dataComp) return;

	std::shared_ptr<SdfField> fieldData = dataComp->GetField();
	if (!fieldData) return;

	GridDesc settings = dataComp->GetGridDesc();
	for (const auto& key : chunks)
	{
		m_backend->PushRequest(BuildRequest{
			.key = key,
			.ptr = requester,
			.fieldData = fieldData,
			.setting = settings
		});
	}
}

void TerrainSystem::SetMode(TerrainMode mode)
{
	if (m_mode != mode || !m_backend)
	{
		m_mode = mode;
		RebuildBackend();
	}
}

#ifdef _DEBUG
GeometryData TerrainSystem::MakeDebugCell(const GridDesc& desc, SdfField* field, bool bDrawAllCells)
{
	GeometryData outMeshData{};
	outMeshData.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

	const int Nx = static_cast<int>(desc.resolution.x);
	const int Ny = static_cast<int>(desc.resolution.y);
	const int Nz = static_cast<int>(desc.resolution.z);

	// XY-Plane
	for (int x = 0; x < Nx; ++x)
	{
		if (!bDrawAllCells && (x > 0 && x < Nx - 1))
		{
			x = Nx - 2;
			continue;
		}
		for (int y = 0; y < Ny; ++y)
		{
			if (!bDrawAllCells && (y > 0 && y < Ny - 1))
			{
				y = Ny - 2;
				continue;
			}

			uint32_t index = static_cast<uint32_t>(outMeshData.indices.size());
			Vertex A{
				.pos = { 
					desc.origin.x + x * desc.cellsize, 
					desc.origin.y + y * desc.cellsize, 
					desc.origin.z 
				},
				.normal = { 0.0f, 0.0f, 1.0f }
			};

			Vertex B{
				.pos = {
					A.pos.x, 
					A.pos.y, 
					A.pos.z + desc.resolution.z * desc.cellsize 
				},
				.normal = { 0.0f, 0.0f, 1.0f }
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
		if (!bDrawAllCells && (x > 0 && x < Nx - 1))
		{
			x = Nx - 2;
			continue;
		}

		for (int z = 0; z < Nz; ++z)
		{
			if (!bDrawAllCells && (z > 0 && z < Nz - 1))
			{
				z = Nz - 2;
				continue;
			}

			uint32_t index = static_cast<uint32_t>(outMeshData.indices.size());
			Vertex A{
				.pos = { 
					desc.origin.x + x * desc.cellsize, 
					desc.origin.y, 
					desc.origin.z + z * desc.cellsize 
				},
				.normal = { 0.0f, 1.0f, 0.0f }
			};

			Vertex B{
				.pos = { 
					A.pos.x, 
					A.pos.y + desc.resolution.y * desc.cellsize,
					A.pos.z 
				},
				.normal = { 0.0f, 0.0f, 0.0f }
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
		if (!bDrawAllCells && (y > 0 && y < Ny - 1))
		{
			y = Ny - 2;
			continue;
		}

		for (int z = 0; z < Nz; ++z)
		{
			if (!bDrawAllCells && (z > 0 && z < Nz - 1))
			{
				z = Nz - 2;
				continue;
			}

			uint32_t index = static_cast<uint32_t>(outMeshData.indices.size());
			Vertex A{
				.pos = { 
					desc.origin.x, 
					desc.origin.y + y * desc.cellsize, 
					desc.origin.z + z * desc.cellsize 
				},
				.normal = { 0.0f, 0.0f, 0.0f }
			};

			Vertex B{
				.pos = { 
					A.pos.x + desc.resolution.x * desc.cellsize, 
					A.pos.y, 
					A.pos.z 
				},
				.normal = { 0.0f, 0.0f, 0.0f }
			};

			outMeshData.vertices.push_back(A);
			outMeshData.vertices.push_back(B);
			outMeshData.indices.push_back(index);
			outMeshData.indices.push_back(index + 1);
		}
	}
	return outMeshData;
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
			m_backend = std::make_unique<GPUTerrainBackend>(device, descriptorAllocator);
		}
		break;
		case TerrainMode::CPU_MC33:
		default:
		{
			m_backend = std::make_unique<CPUTerrainBackend>(device);
		}
		break;
	}
}