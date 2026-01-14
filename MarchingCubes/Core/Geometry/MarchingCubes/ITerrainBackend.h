#pragma once
#include <Core/Geometry/MarchingCubes/SdfField.h>
#include "Core/DataStructures/Data.h"
#include <set>

enum class TerrainMode
{
	CPU_MC33,
	GPU_ORIGINAL
};

struct GridDesc
{
	DirectX::XMUINT3 resolution = { 0u, 0u,0u };
	float cellsize = 0.0f;
	DirectX::XMFLOAT3 origin = {0.0f, 0.0f, 0.0f};
	uint32_t chunkSize = 0;
	float isoValue = 0.0f;
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

struct BrushRequest
{
	float deltaTime = 0.016f;
	DirectX::XMFLOAT3 center{};
	float radius = 1.0f;
	float weight = 1.0f;
};

struct ITerrainBackend
{
	virtual ~ITerrainBackend() = default;
	virtual void setGridDesc(const GridDesc& desc) = 0;
	virtual void setFieldPtr(std::shared_ptr<SdfField> grid) = 0;	// GPU: density3D 갱신 / CPU: 내부 GRD 보관
	virtual void RequestBrush(const BrushRequest& r) = 0;
	virtual void RequestRemesh(const std::set<ChunkKey>& chunkSet) = 0;
	virtual bool tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates) = 0;  // GPU : readback / CPU : GeometryData -> GeometryBuffer Commit
	virtual bool HasRequests() const = 0;
};