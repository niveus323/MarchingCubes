#include "pch.h"
#include "CPUTerrainBackend.h"
#include "Core/Math/PhysicsHelper.h"
#include <algorithm>
#include "Core/Utils/Log.h"

CPUTerrainBackend::CPUTerrainBackend(ID3D12Device* device, const GridDesc& desc):
    m_gridDesc(desc),
    m_chunkSize(std::max(desc.cells.x, std::max(desc.cells.y, desc.cells.z)))
{
}

void CPUTerrainBackend::setGridDesc(const GridDesc& desc)
{
	m_gridDesc = desc;
    m_chunkSize = std::max(desc.cells.x, std::max(desc.cells.y, desc.cells.z));
}

void CPUTerrainBackend::setFieldPtr(std::shared_ptr<SdfField<float>> grid)
{
	m_grd = std::move(grid);
}

void CPUTerrainBackend::requestBrush(const BrushRequest& req)
{
    RemeshRequest remeshRequest;
    remeshRequest.isoValue = req.isoValue;

    const XMUINT3 cells = m_gridDesc.cells;
    const XMFLOAT3 origin = m_gridDesc.origin;
    const float cellsize = m_gridDesc.cellsize;

    const float deltaTime = req.deltaTime;
    const XMFLOAT3 hitPos = req.hitpos;
    const float weight = req.weight;
    const float radius = req.radius;

    const int SX = int(m_gridDesc.cells.x) + 1;
    const int SY = int(m_gridDesc.cells.y) + 1;
    const int SZ = int(m_gridDesc.cells.z) + 1;

    const float kBase = std::clamp(m_brushDelta * deltaTime * std::abs(weight), 0.0f, 1.0f);

    // 영향 범위 (Field 인덱스 공간으로 변환)
    auto sample = [cellsize](float p, float o) { return (p - o) / cellsize; };
    int minX = std::max(0, int(std::floor(sample(hitPos.x - radius, origin.x))));
    int maxX = std::min(SX - 1, int(std::ceil(sample(hitPos.x + radius, origin.x))));
    int minY = std::max(0, int(std::floor(sample(hitPos.y - radius, origin.y))));
    int maxY = std::min(SY - 1, int(std::ceil(sample(hitPos.y + radius, origin.y))));
    int minZ = std::max(0, int(std::floor(sample(hitPos.z - radius, origin.z))));
    int maxZ = std::min(SZ - 1, int(std::ceil(sample(hitPos.z + radius, origin.z))));

    for (int z = minZ; z <= maxZ; ++z)
    {
        const float pz = origin.z + z * cellsize;
        const float dz = pz - hitPos.z;

        for (int y = minY; y <= maxY; ++y)
        {
            const float py = origin.y + y * cellsize;
            const float dy = py - hitPos.y;

            for (int x = minX; x <= maxX; ++x)
            {
                const float px = origin.x + x * cellsize;
                const float dx = px - hitPos.x;

                const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > radius) continue; // 반경 밖은 영향 없음(빠른 스킵)

                // Brush 중심과의 거리에 따라 가중치 부여
                const float sphere = radius - dist;

                float& F = m_grd->at(x, y, z);
                float desired = (weight < 0) ? std::min(F, -sphere) : std::max(F, sphere);
                const float falloff = std::clamp(sphere / radius, 0.0f, 1.0f);
                const float k = kBase * falloff;

                F = F + (desired - F) * k;

                remeshRequest.chunkset.insert(ChunkKey{ x / m_chunkSize, y / m_chunkSize, z / m_chunkSize });
            }
        }
    }

    requestRemesh(remeshRequest);
}

bool CPUTerrainBackend::tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates)
{
	// CPU Terrain은 단일 Chunk.
	OutChunkUpdates.clear();
	ChunkUpdate up;
	up.empty = m_meshData.indices.empty();
	up.key = { 0,0,0 };
	up.md = std::move(m_meshData);
	OutChunkUpdates.push_back(up);

	return !up.empty;
}
