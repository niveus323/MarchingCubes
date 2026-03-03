#include "pch.h"
#include "MeshRegistry.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/Memory/GpuAllocator.h"
#include "Core/Geometry/Mesh/Class/StaticMesh.h"
#include "Core/Geometry/Mesh/Class/DynamicMesh.h"
#include "Core/Assets/MeshAsset.h"

std::shared_ptr<StaticMesh> MeshRegistry::CreateStaticMesh(const std::string& key, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes)
{
	auto resuIt = m_resourceCache.find(key);
	if (resuIt != m_resourceCache.end()) return resuIt->second;

	auto allocator = EngineCore::GetGpuAllocator();
	auto uploadContext = EngineCore::GetUploadContext();
	assert(allocator && uploadContext);

	GeometryBuffer geoBuffer;
	if (allocator)
	{
		BufferHandle vbHandle;
		uint64_t vbSize = data.vertices.size() * sizeof(Vertex);
		if (vbSize > 0)
		{
			allocator->Alloc(AllocDesc::Static(vbSize, key + "_VB"), vbHandle);
			geoBuffer.SwapVBHandle(vbHandle);
		}

		BufferHandle ibHandle;
		uint64_t ibSize = data.indices.size() * sizeof(uint32_t);
		if (ibSize > 0)
		{
			allocator->Alloc(AllocDesc::Static(ibSize, key + "_IB"), ibHandle);
			geoBuffer.SwapIBHandle(ibHandle);
		}
		uploadContext->UploadGeometry(&geoBuffer, data, key);
	}

	auto mesh = std::make_shared<StaticMesh>(key);
	mesh->Initialize(geoBuffer, data, submeshes);

	m_resourceCache[key] = mesh;
	return mesh;
}

std::shared_ptr<StaticMesh> MeshRegistry::CreateStaticMesh(const std::shared_ptr<MeshAsset>& asset)
{
	if (!asset) return nullptr;

	std::string key = asset->GetSourcePath().string();
	auto resIt = m_resourceCache.find(key);
	if (resIt != m_resourceCache.end()) return resIt->second;

	return CreateStaticMesh(key, asset->GetGeometry(), asset->GetSubmesh());
}

std::shared_ptr<DynamicMesh> MeshRegistry::CreateDynamicMesh(const GeometryData& data, const std::vector<MeshSubmesh>& submeshes, const std::string& debugName)
{
	auto mesh = std::make_shared<DynamicMesh>(debugName);
	UpdateDynamicMesh(mesh, data, submeshes);
	return mesh;
}

void MeshRegistry::UpdateDynamicMesh(std::shared_ptr<DynamicMesh> mesh, const GeometryData& newData, const std::vector<MeshSubmesh>& submeshes)
{
	auto uploadContext = EngineCore::GetUploadContext();
	assert(uploadContext);

	if (!mesh) return;

	GeometryBuffer& oldBuffer = *mesh->GetGPUBuffer();
	if (oldBuffer.GetVBHandle().res != nullptr || oldBuffer.GetIBHandle().res != nullptr)
	{
		uploadContext->FreeGeometryBuffer(oldBuffer);
	}
	
	auto allocator = EngineCore::GetGpuAllocator();
	std::string meshName = std::string(mesh->GetDebugName());
	GeometryBuffer newGeoBuffer;
	uint64_t vbSize = newData.vertices.size() * sizeof(Vertex);
	if (vbSize > 0)
	{
		BufferHandle vbHandle{};
		allocator->Alloc(AllocDesc::Dynamic(vbSize, meshName + "_VB"), vbHandle);
		newGeoBuffer.SwapVBHandle(vbHandle);
	}

	uint64_t ibSize = newData.indices.size() * sizeof(uint32_t);
	if (ibSize > 0)
	{
		BufferHandle ibHandle{};
		allocator->Alloc(AllocDesc::Dynamic(ibSize, meshName + "_IB"), ibHandle);
		newGeoBuffer.SwapIBHandle(ibHandle);
	}

	DXGI_FORMAT ibFormat = (sizeof(uint32_t) == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	newGeoBuffer.SetLayout(sizeof(Vertex), ibFormat);

	uploadContext->UploadGeometry(&newGeoBuffer, newData, mesh->GetDebugName());

	mesh->SwapBuffer(newGeoBuffer, newData, submeshes);
}