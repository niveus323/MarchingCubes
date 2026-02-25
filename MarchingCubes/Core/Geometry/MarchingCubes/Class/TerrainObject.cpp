#include "pch.h"
#include "TerrainObject.h"
#include "Core/Scene/Scene.h"
#include "Core/Engine/Subsystem/SceneSubsystem/TerrainSystem.h"
#include "Core/Geometry/MarchingCubes/Class/SdfDataComponent.h"
#include "Core/Geometry/Mesh/Component/CustomMeshComponent.h"
#include "Core/Geometry/Mesh/Component/StaticMeshComponent.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Assets/Material/MaterialAsset.h"
#include <format>

BEGIN_REFLECTION(TerrainObject, SceneObject)
	REFLECT_PROPERTY(m_dataPath, EPropertyType::String)
END_REFLECTION()

void TerrainObject::Init()
{
	SceneObject::Init();
	m_terrainSystem = GetScene()->AddSubsystem<TerrainSystem>();
	m_dataComponent = AddComponent<SdfDataComponent>(EObjectFlags::Transient);
	m_material = EngineCore::GetResourceManager()->LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/DefaultFilled.json"));
#ifdef _DEBUG
	m_entireBoundingCube = CreateChild<SceneObject>("TerrainBound", EObjectFlags::Transient | EObjectFlags::EditorOnly | EObjectFlags::Invisible);
	auto cubeMeshComp = m_entireBoundingCube->AddComponent<StaticMeshComponent>();
	cubeMeshComp->SetName("BoundCube");
	cubeMeshComp->SetMeshByPath("@WireCube");
	m_entireBoundingCube->SetActive(false);
#endif // _DEBUG

}

void TerrainObject::Destroy()
{
	SceneObject::Destroy();
	m_chunks.clear();
}

void TerrainObject::BeginPlay()
{
	SceneObject::BeginPlay();
}

void TerrainObject::EndPlay()
{
	SceneObject::EndPlay();
}

void TerrainObject::OnPreSave()
{
	if (IsDirty())
	{
		SaveDataAsset(m_dataPath);
	}
}

void TerrainObject::Serialize(Serializer& ar)
{
	SceneObject::Serialize(ar);

	std::string materialPath = (m_material)? m_material->GetPath().data() : "";
	ar.Serialize("Material", materialPath);

	if (!ar.IsSaving() && !materialPath.empty())
	{
		m_material = EngineCore::GetResourceManager()->LoadMaterialAsset(materialPath);
	}
	
	ar.Serialize("DataPath", m_dataPath);
	if (!ar.IsSaving())
	{
		ThrowIfFalse(LoadDataAsset(m_dataPath), std::format("TerrainObject : Faile to Load Data {}", m_dataPath));
	}
}

std::shared_ptr<DataAsset> TerrainObject::CreateDataAsset() const
{
	auto data = m_dataComponent->GetField();
	auto& settings = m_dataComponent->GetGridDesc();
	
	// Blob
	size_t headerSize = sizeof(TerrainDataLayout);
	size_t bodySize = data->size() * sizeof(float);
	std::vector<uint8_t> blob(headerSize + bodySize);

	// Header
	TerrainDataLayout* layout = reinterpret_cast<TerrainDataLayout*>(blob.data());
	layout->desc = settings;

	// Body
	std::memcpy(blob.data() + headerSize, data->data(), bodySize);

	return std::make_shared<DataAsset>(EDataAssetType::TerrainSDF, blob.data(), blob.size());
}

bool TerrainObject::SaveDataAsset(std::string_view path)
{
	if (auto asset = CreateDataAsset()) // 현재 데이터를 DataAsset으로 packing
	{
		if (EngineCore::GetResourceManager()->SaveDataAsset(path, asset))
		{
			SetDirty(false);
			return true;
		}

		Log::Print("TerrainObject", "Failed To Save Data Asset!!!!\n Desired Path : %s", path.data());
		return false;
	}

	Log::Print("TerrainObject", "Failed To Create Data Asset!!!!");
	return false;
}

bool TerrainObject::LoadDataAsset(std::string_view path)
{
	if (path.empty()) return false;
	
	m_dataPath = std::string(path);
	if (auto asset = EngineCore::GetResourceManager()->LoadDataAsset(m_dataPath))
	{
		if (asset->GetType() != EDataAssetType::TerrainSDF)
		{
			Log::Print("TerrainSystem", "Invalid Asset Type!!!! file : %s", m_dataPath);
			return false;
		}

		if (const uint8_t* rawBlob = asset->GetRawData())
		{
			const auto* layout = reinterpret_cast<const TerrainDataLayout*>(rawBlob);
			auto setting = &layout->desc;
			auto newField = std::make_shared<SdfField>(setting->resolution.x + 1, setting->resolution.y + 1, setting->resolution.z + 1);
			const float* srcFloats = reinterpret_cast<const float*>(rawBlob + sizeof(TerrainDataLayout));
			size_t voxelCount = newField->size();

			// 유효성 검사 (데이터 크기가 맞는지)
			size_t expectedSize = voxelCount * sizeof(float);
			if (asset->GetSize() < sizeof(TerrainDataLayout) + expectedSize)
			{
				Log::Print("TerrainSystem", "Mismatched DataAsset!!!!");
				return false;
			}

			std::memcpy(newField->data(), srcFloats, expectedSize);

			m_dataComponent->SetGridDesc(*setting);
			m_dataComponent->SetData(std::move(newField));
			BuildMesh(GetChunkKeys());
		}
	}
	return true;
}

void TerrainObject::InjectTerrain(const GridDesc& desc, std::shared_ptr<SdfField> field)
{
	if (!field) return;

	m_dataComponent->SetGridDesc(desc);
	m_dataComponent->SetData(field);
	BuildMesh(GetChunkKeys());
}

void TerrainObject::ApplyBrush(float brushDelta, const DirectX::XMFLOAT3& pos, const float radius)
{
	auto data = m_dataComponent->GetField();

	auto& setting = m_dataComponent->GetGridDesc();
	const XMUINT3 resolution = setting.resolution;
	const XMFLOAT3 origin = setting.origin;
	const float cellsize = setting.cellsize;

	const int SX = int(setting.resolution.x);
	const int SY = int(setting.resolution.y);
	const int SZ = int(setting.resolution.z);

	const float clampedDeltaAbs = std::clamp(std::abs(brushDelta), 0.0f, 1.0f);

	// 영향 범위 (Field 인덱스 공간으로 변환)
	auto sample = [cellsize](float p, float o) { return (p - o) / cellsize; };
	int minX = std::max(0, int(std::floor(sample(pos.x - radius, origin.x))));
	int maxX = std::min(SX - 1, int(std::ceil(sample(pos.x + radius, origin.x))));
	int minY = std::max(0, int(std::floor(sample(pos.y - radius, origin.y))));
	int maxY = std::min(SY - 1, int(std::ceil(sample(pos.y + radius, origin.y))));
	int minZ = std::max(0, int(std::floor(sample(pos.z - radius, origin.z))));
	int maxZ = std::min(SZ - 1, int(std::ceil(sample(pos.z + radius, origin.z))));

	std::set<ChunkKey> chunkset;
	for (int z = minZ; z <= maxZ; ++z)
	{
		const float pz = origin.z + z * cellsize;
		const float dz = pz - pos.z;

		for (int y = minY; y <= maxY; ++y)
		{
			const float py = origin.y + y * cellsize;
			const float dy = py - pos.y;

			for (int x = minX; x <= maxX; ++x)
			{
				const float px = origin.x + x * cellsize;
				const float dx = px - pos.x;

				const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
				if (dist > radius) continue; // 반경 밖은 영향 없음(빠른 스킵)

				// Brush 중심과의 거리에 따라 가중치 부여
				const float sphere = radius - dist;

				float& F = data->at(x, y, z);
				float desired = (brushDelta < 0) ? std::min(F, -sphere) : std::max(F, sphere);
				const float falloff = std::clamp(sphere / radius, 0.0f, 1.0f);
				const float k = clampedDeltaAbs * falloff;

				F = F + (desired - F) * k;

				chunkset.insert(ChunkKey{ x / setting.cellsPerChunk, y / setting.cellsPerChunk,  z / setting.cellsPerChunk });
				
			}
		}
	}

	SetDirty(true);
	BuildMesh(chunkset);
}

void TerrainObject::ApplyChunkMesh(ChunkKey key, std::vector<Vertex> vertices, std::vector<uint32_t> indices)
{
	auto it = m_chunks.find(key);
	if (it == m_chunks.end()) return;

	ChunkInfo& info = it->second;
	if (info.meshComponent)
	{
		MeshSubmesh submesh{
			.indexCount = static_cast<uint32_t>(indices.size()),
			.indexOffset = 0,
			.baseVertexLocation = 0,
			.materialslot = 0
		};

		GeometryData data{
			.vertices = vertices,
			.indices = indices
		};

		info.meshComponent->UpdateMesh(data, { submesh });
	}
}

void TerrainObject::ShowChunkBounds(bool bShow)
{
#ifdef _DEBUG
	m_bShowChunkBounds = true;
	for (auto& [key, info] : m_chunks)
	{
		info.meshComponent->SetOverlayPassActive("DebugChunkBound", bShow);
	}
#endif
}

void TerrainObject::ShowTerrainBound(bool bShow)
{
#ifdef _DEBUG
	m_entireBoundingCube->SetActive(bShow);
	if (m_entireBoundingCube->IsActive())
	{
		const GridDesc& settings = GetSetting();

		// 지형 크기 계산
		float sizeX = settings.resolution.x * settings.cellsize;
		float sizeY = settings.resolution.y * settings.cellsize;
		float sizeZ = settings.resolution.z * settings.cellsize;

		// 중심점 계산
		float centerX = settings.origin.x + sizeX * 0.5f;
		float centerY = settings.origin.y + sizeY * 0.5f;
		float centerZ = settings.origin.z + sizeZ * 0.5f;

		// 1x1x1 큐브를 -> 지형 크기만큼 확대, 지형 중심으로 이동
		m_entireBoundingCube->SetScale({ sizeX, sizeY, sizeZ });
		m_entireBoundingCube->SetPosition({ centerX, centerY, centerZ });
	}
#endif // _DEBUG
}

void TerrainObject::ShowWireframe(bool bShow)
{
#ifdef _DEBUG
	for (auto [key, info] : m_chunks)
	{
		info.meshComponent->SetPSO(bShow ? "Wire" : "Filled");
	}
#endif // _DEBUG
}

void TerrainObject::SetSetting(const GridDesc& desc)
{
	m_dataComponent->SetGridDesc(desc);
}

void TerrainObject::BuildMesh(std::set<ChunkKey> chunks)
{
	if (chunks.empty())
		return;

	if (m_terrainSystem) m_terrainSystem->RequestChunkRemesh(this->GetSharedPtr<TerrainObject>(), chunks);
}

void TerrainObject::OnSettingUpdated()
{
	// 해상도 변경 시 청크 수정
	const auto& setting = m_dataComponent->GetGridDesc();

	const uint32_t chunkCountX = setting.resolution.x / setting.cellsPerChunk;
	const uint32_t chunkCountY = setting.resolution.y / setting.cellsPerChunk;
	const uint32_t chunkCountZ = setting.resolution.z / setting.cellsPerChunk;

	m_chunkBorderConstants.gridOrigin = setting.origin;
	// NOTE : 청크 크기를 x,y,z마다 다르게 하고 싶을 경우를 고려해서 float3로 두었음
	m_chunkBorderConstants.chunkWorldSize = XMFLOAT3{ 
		setting.cellsPerChunk * setting.cellsize,
		setting.cellsPerChunk * setting.cellsize,
		setting.cellsPerChunk * setting.cellsize
	};

	if (auto uploadContext = EngineCore::GetUploadContext())
	{
		uploadContext->UploadDynamicConstants(&m_chunkBorderConstants, sizeof(ChunkBoundaryConstants), m_chunkBorderCB, this);
	}

	std::vector<ShaderBinding> newBinding;
	newBinding.push_back(ShaderBinding{ 
		.type = EBindingType::CBV, 
		.rootParameterIndex = 6, // b3
		.gpuAddress = m_chunkBorderCB.gpuVA
	});
	
	// 더 이상 필요 없는 청크는 제거
	for (auto iter = m_chunks.begin(); iter != m_chunks.end();)
	{
		if (!IsKeyValid(iter->first))
		{
			//erase하면서 컴포넌트 리스트에서 제외해야함.
			auto uuid = iter->second.meshComponent->GetUUID();
			iter = m_chunks.erase(iter);
			UnregisterComponent(uuid);
		}
		else
		{
			iter++;
		}
	}

	std::unordered_map<ChunkKey, ChunkInfo, ChunkKeyHash> newChunkMap;
	newChunkMap.reserve(chunkCountX * chunkCountY * chunkCountZ);
	for (uint32_t chunkX = 0; chunkX < chunkCountX; ++chunkX)
	{
		for (uint32_t chunkY = 0; chunkY < chunkCountY; ++chunkY)
		{
			for (uint32_t chunkZ = 0; chunkZ < chunkCountZ; ++chunkZ)
			{
				ChunkKey key = { chunkX, chunkY, chunkZ };
				ChunkInfo info;
				if (m_chunks.find(key) != m_chunks.end())
				{
					// 변경 전에 이미 생성해둔 청크가 있으면 가져온다
					info = std::move(m_chunks[key]);
					info.meshComponent->AddOverlayPass("DebugChunkBound", "DrawChunkBoundary", newBinding, m_bShowChunkBounds); // 변경 후 CBV 가상 메모리 주소 갱신을 위해 새로 바인딩
				}
				else
				{
					auto chunkMeshComp = AddComponent<CustomMeshComponent>(EObjectFlags::Transient);
					std::string compName = std::format("TerrainMeshComponent({0},{1},{2})", key.x, key.y, key.z);
					chunkMeshComp->SetName(compName);
					chunkMeshComp->SetMaterial(0, m_material);
					chunkMeshComp->AddOverlayPass("DebugChunkBound", "DrawChunkBoundary", newBinding, m_bShowChunkBounds);
					info = ChunkInfo{ .meshComponent = chunkMeshComp };
				}
				newChunkMap.insert_or_assign(key, info);
			}
		}
	}
	m_chunks = std::move(newChunkMap);

#ifdef _DEBUG
	if (m_entireBoundingCube && m_entireBoundingCube->IsActive())
	{
		ShowTerrainBound(true);
	}
#endif
}

std::set<ChunkKey> TerrainObject::GetChunkKeys()
{
	auto& setting = m_dataComponent->GetGridDesc();

	std::set<ChunkKey> chunkSet;
	uint32_t chunkX = setting.resolution.x / setting.cellsPerChunk;
	uint32_t chunkY = setting.resolution.y / setting.cellsPerChunk;
	uint32_t chunkZ = setting.resolution.z / setting.cellsPerChunk;
	for (uint32_t x = 0; x < chunkX; ++x)
		for (uint32_t y = 0; y < chunkY; ++y)
			for (uint32_t z = 0; z < chunkZ; ++z)
				chunkSet.insert(ChunkKey{ x,y,z });

	return chunkSet;
}

bool TerrainObject::IsKeyValid(const ChunkKey& key)
{
	const GridDesc& desc = m_dataComponent->GetGridDesc();
	uint32_t maxX = (desc.resolution.x + desc.cellsPerChunk - 1) / desc.cellsPerChunk;
	uint32_t maxY = (desc.resolution.y + desc.cellsPerChunk - 1) / desc.cellsPerChunk;
	uint32_t maxZ = (desc.resolution.z + desc.cellsPerChunk - 1) / desc.cellsPerChunk;

	return (key.x < maxX && key.y < maxY && key.z < maxZ);
}
