#pragma once
//#include <MC33_c/marching_cubes_33.h>
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
	DirectX::XMUINT3 cells;
	float cellsize;
	DirectX::XMFLOAT3 origin;
};

struct ChunkKey
{
	UINT x = 0;
	UINT y = 0;
	UINT z = 0;

	inline bool operator==(const ChunkKey& key) const noexcept
	{
		return x == key.x && y == key.y && z == key.z;
	}

	inline bool operator<(const ChunkKey& rhs) const noexcept
	{
		return (x == rhs.x ? (y == rhs.y ? (z < rhs.z) : y < rhs.y) : x < rhs.x);
	}
};

struct ChunkUpdate
{
	ChunkKey key{};
	MeshData md{};
	bool empty = true;
};

struct RemeshRequest
{
	float isoValue = 0.0f;
	std::set<ChunkKey> chunkset;
};

struct BrushRequest
{
	DirectX::XMFLOAT3 hitpos{};
	float radius = 1.0f;
	float weight = 1.0f;
	float deltaTime = 0.016f;
	float isoValue = 0.0f;
};

struct ITerrainBackend
{
	virtual ~ITerrainBackend() = default;
	virtual void setGridDesc(const GridDesc&) = 0;
	virtual void setFieldPtr(std::shared_ptr<SdfField<float>> grid) = 0;			// GPU: density3D 갱신 / CPU: 내부 GRD 보관
	virtual void requestBrush(const BrushRequest&) = 0;
	virtual void requestRemesh(const RemeshRequest&) = 0;
	virtual bool tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates) = 0;  // GPU : readback / CPU : MeshData -> MeshBuffer Commit
};