#pragma once
#include <Core/Geometry/MarchingCubes/SdfField.h>
#include "Core/DataStructures/Data.h"
#include "Core/Engine/Serializer/Serializer.h"

enum class TerrainMode
{
	CPU_MC33,
	GPU_ORIGINAL
};

struct GridDesc
{
	DirectX::XMUINT3 resolution = { 100u, 100u, 100u };
	float cellsize = 1.0f; //TODO : 惑荐贸府
	DirectX::XMFLOAT3 origin = {0.0f, 0.0f, 0.0f}; // TODO : 惑荐贸府
	uint32_t cellsPerChunk = 50u;
	float isoValue = 0.0f;

	void Serialize(Serializer& ar)
	{
		ar.Serialize("Resolution", resolution);
		ar.Serialize("CellSize", cellsize);
		ar.Serialize("Origin", origin);
		ar.Serialize("ChunkSize", cellsPerChunk);
		ar.Serialize("IsoValue", isoValue);
	}
};

struct ChunkKey
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;

	inline bool operator==(const ChunkKey& key) const noexcept
	{
		return x == key.x && y == key.y && z == key.z;
	}

	inline bool operator<(const ChunkKey& rhs) const noexcept
	{
		return (x == rhs.x ? (y == rhs.y ? (z < rhs.z) : y < rhs.y) : x < rhs.x);
	}
};

struct ChunkKeyHash {
	size_t operator()(const ChunkKey& k) const noexcept {
		return (size_t)k.x ^ ((size_t)k.y << 21) ^ ((size_t)k.z << 42);
	}
};

struct ChunkUpdate
{
	ChunkKey key{};
	GeometryData md{};
	bool empty = true;
};

//Forward Declaration
class TerrainObject;

struct BuildRequest
{
	ChunkKey key;
	std::weak_ptr<TerrainObject> ptr;
	std::shared_ptr<SdfField> fieldData;
	GridDesc setting;
};

struct BuildResult
{
	ChunkKey key;
	std::weak_ptr<TerrainObject> ptr;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

struct ITerrainBackend
{
	virtual ~ITerrainBackend() = default;
	virtual void PushRequest(BuildRequest&& request) = 0;
	virtual bool TryFetch(std::vector<BuildResult>& OutResults) = 0;
	virtual bool HasRequests() const = 0;
};