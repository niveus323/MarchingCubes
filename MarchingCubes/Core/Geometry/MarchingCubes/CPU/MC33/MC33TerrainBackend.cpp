#include "pch.h"
#include "MC33TerrainBackend.h"
#include <MC33_c/marching_cubes_33.h>
#include <cmath>

#include <ppl.h> 
#include <mutex>

void MC33TerrainBackend::RequestRemesh(const std::set<ChunkKey>& chunkset)
{
    std::vector<ChunkKey> keys(chunkset.begin(), chunkset.end());

    std::mutex dataMutex;
    concurrency::parallel_for_each(keys.begin(), keys.end(), [&](const ChunkKey& chunkKey) {
        _GRD localGrd{};
        int chunkSize = m_gridDesc.chunkSize;
        const int baseX = chunkKey.x * chunkSize;
        const int baseY = chunkKey.y * chunkSize;
        const int baseZ = chunkKey.z * chunkSize;

        localGrd.N[0] = chunkSize;
        localGrd.N[1] = chunkSize;
        localGrd.N[2] = chunkSize;

        localGrd.d[0] = static_cast<double>(m_gridDesc.cellsize);
        localGrd.d[1] = static_cast<double>(m_gridDesc.cellsize);
        localGrd.d[2] = static_cast<double>(m_gridDesc.cellsize);

        localGrd.r0[0] = static_cast<double>(m_gridDesc.origin.x + static_cast<float>(baseX) * m_gridDesc.cellsize);
        localGrd.r0[1] = static_cast<double>(m_gridDesc.origin.y + static_cast<float>(baseY) * m_gridDesc.cellsize);
        localGrd.r0[2] = static_cast<double>(m_gridDesc.origin.z + static_cast<float>(baseZ) * m_gridDesc.cellsize);

        localGrd.nonortho = 0;
        localGrd.periodic = 0;

        SdfField<float> chunk(chunkSize + 1, chunkSize + 1, chunkSize + 1);
        for (int z = 0; z <= chunkSize; ++z) 
        {
            for (int y = 0; y <= chunkSize; ++y) 
            {
                // 읽기는 Thread-Safe (m_grd가 const라면)
                const float* srcRow = m_grd->rowPtr(baseY + y, baseZ + z) + baseX;
                float* dstRow = chunk.rowPtr(y, z);
                std::memcpy(dstRow, srcRow, static_cast<size_t>(chunkSize + 1) * sizeof(float));
            }
        }
        localGrd.F = reinterpret_cast<GRD_data_type***>(static_cast<float***>(chunk));

        MC33* M = create_MC33(&localGrd);
        surface* S = calculate_isosurface(M, m_gridDesc.isoValue);

        GeometryData tempData;
        tempData.vertices.reserve(S->nV);
        for (unsigned i = 0; i < S->nV; ++i)
        {
            float* p = S->V[i];
            float* n = S->N[i];

            XMVECTOR N = XMVector3Normalize(XMVectorSet(n[0], n[1], n[2], 0.0f));

            // N과 너무 평행하지 않은 기준 축 선택
            float ny = XMVectorGetY(N);
            XMVECTOR up = (fabsf(ny) > 0.999f) ? XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            // T = up × N (정규화)
            XMVECTOR T = XMVector3Normalize(XMVector3Cross(up, N));

            XMFLOAT3 t3;
            XMStoreFloat3(&t3, T);

            tempData.vertices.push_back(Vertex{
                .pos = { p[0], p[1], p[2] },
                .normal = { n[0], n[1], n[2] },
                .tangent = { t3.x, t3.y, t3.z, 1.0f },
                .color = {1.0f, 1.0f, 1.0f, 1.0f}
                });
        }
        m_chunkData[chunkKey].indices.reserve(S->nT * 3);
        for (unsigned t = 0; t < S->nT; ++t)
        {
            tempData.indices.push_back(S->T[t][0]);
            tempData.indices.push_back(S->T[t][1]);
            tempData.indices.push_back(S->T[t][2]);
        }

        free_surface_memory(S);
        free_MC33(M);

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            m_chunkData[chunkKey] = std::move(tempData);
        }
    });
    
}
