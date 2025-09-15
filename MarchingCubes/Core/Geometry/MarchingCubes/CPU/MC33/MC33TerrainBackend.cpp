#include "pch.h"
#include "MC33TerrainBackend.h"
#include "Core/Math/PhysicsHelper.h"
#include <algorithm>

void MC33TerrainBackend::requestBrush(const BrushRequest& req)
{
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
    auto sample = [&](float p, float o) { return (p - o) / cellsize; };
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

                float& F = m_grd->F[z][y][x];
                float desired = (weight < 0) ? std::min(F, -sphere) : std::max(F, sphere);
                const float falloff = std::clamp(sphere / radius, 0.0f, 1.0f);
                const float k = kBase * falloff;

                F = F + (desired - F) * k;
            }
        }
    }

	RemeshRequest remeshRequest;
	remeshRequest.isoValue = req.isoValue;
	requestRemesh(remeshRequest);
}

void MC33TerrainBackend::requestRemesh(const RemeshRequest& req)
{
    m_meshData.vertices.clear();
    m_meshData.indices.clear();

    MC33* M = create_MC33(m_grd.get());
    surface* S = calculate_isosurface(M, req.isoValue);

    m_meshData.vertices.reserve(S->nV);
    for (unsigned i = 0; i < S->nV; ++i) {
        float* p = S->V[i];
        float* n = S->N[i];
        m_meshData.vertices.push_back({ { p[0], p[1], p[2] }, { n[0], n[1], n[2] }, {1.0f, 1.0f, 1.0f, 1.0f} });
    }
    m_meshData.indices.reserve(S->nT * 3);
    for (unsigned t = 0; t < S->nT; ++t) {
        m_meshData.indices.push_back(S->T[t][0]);
        m_meshData.indices.push_back(S->T[t][1]);
        m_meshData.indices.push_back(S->T[t][2]);
    }

    free_surface_memory(S);
    free_MC33(M);
}
