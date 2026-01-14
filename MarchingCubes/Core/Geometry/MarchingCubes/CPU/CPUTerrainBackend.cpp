#include "pch.h"
#include "CPUTerrainBackend.h"
#include <algorithm>
#include <cmath>

CPUTerrainBackend::CPUTerrainBackend(ID3D12Device* device, const GridDesc& desc):
    m_gridDesc(desc)
{
}

void CPUTerrainBackend::setGridDesc(const GridDesc& desc)
{
	m_gridDesc = desc;
}

void CPUTerrainBackend::setFieldPtr(std::shared_ptr<SdfField> grid)
{
	m_grd = std::move(grid);
}

void CPUTerrainBackend::RequestBrush(const BrushRequest& r)
{
    std::set<ChunkKey> chunkset;
    
    const XMUINT3 resolution = m_gridDesc.resolution;
    const XMFLOAT3 origin = m_gridDesc.origin;
    const float cellsize = m_gridDesc.cellsize;

    const float deltaTime = r.deltaTime;
    const XMFLOAT3 center = r.center;
    const float weight = r.weight;
    const float radius = r.radius;

    const int SX = int(m_gridDesc.resolution.x);
    const int SY = int(m_gridDesc.resolution.y);
    const int SZ = int(m_gridDesc.resolution.z);

    const float kBase = std::clamp(m_brushDelta * deltaTime * std::abs(weight), 0.0f, 1.0f);

    // 영향 범위 (Field 인덱스 공간으로 변환)
    auto sample = [cellsize](float p, float o) { return (p - o) / cellsize; };
    int minX = std::max(0, int(std::floor(sample(center.x - radius, origin.x))));
    int maxX = std::min(SX - 1, int(std::ceil(sample(center.x + radius, origin.x))));
    int minY = std::max(0, int(std::floor(sample(center.y - radius, origin.y))));
    int maxY = std::min(SY - 1, int(std::ceil(sample(center.y + radius, origin.y))));
    int minZ = std::max(0, int(std::floor(sample(center.z - radius, origin.z))));
    int maxZ = std::min(SZ - 1, int(std::ceil(sample(center.z + radius, origin.z))));

    for (int z = minZ; z <= maxZ; ++z)
    {
        const float pz = origin.z + z * cellsize;
        const float dz = pz - center.z;

        for (int y = minY; y <= maxY; ++y)
        {
            const float py = origin.y + y * cellsize;
            const float dy = py - center.y;

            for (int x = minX; x <= maxX; ++x)
            {
                const float px = origin.x + x * cellsize;
                const float dx = px - center.x;

                const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > radius) continue; // 반경 밖은 영향 없음(빠른 스킵)

                // Brush 중심과의 거리에 따라 가중치 부여
                const float sphere = radius - dist;

                float& F = m_grd->at(x, y, z);
                float desired = (weight < 0) ? std::min(F, -sphere) : std::max(F, sphere);
                const float falloff = std::clamp(sphere / radius, 0.0f, 1.0f);
                const float k = kBase * falloff;

                F = F + (desired - F) * k;

                chunkset.insert(ChunkKey{ x / m_gridDesc.chunkSize, y / m_gridDesc.chunkSize,  z / m_gridDesc.chunkSize });
            }
        }
    }

    RequestRemesh(chunkset);
}

bool CPUTerrainBackend::tryFetch(std::vector<ChunkUpdate>& OutChunkUpdates)
{
	OutChunkUpdates.clear();
    for (auto& [key, data] : m_chunkData)
    {
        ChunkUpdate up;
        up.empty = data.indices.empty();
        up.key = key;
        up.md = std::move(data);
        OutChunkUpdates.push_back(up);
    }
    m_chunkData.clear();

    return !OutChunkUpdates.empty();
}
