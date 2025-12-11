#include "pch.h"
#include "MC33TerrainBackend.h"
#include <MC33_c/marching_cubes_33.h>
#include <cmath>

void MC33TerrainBackend::requestRemesh(uint32_t frameIndex, const RemeshRequest& r)
{
    m_chunkData.clear();

    int chunkSize = m_gridDesc.chunkSize;
    const int totalX = m_grd->sx();
    const int totalY = m_grd->sy();
    const int totalZ = m_grd->sz();

    _GRD* grd = new _GRD{};
    for (auto& chunkKey : r.chunkset)
    {
        const int baseX = chunkKey.x * chunkSize;
        const int baseY = chunkKey.y * chunkSize;
        const int baseZ = chunkKey.z * chunkSize;

        grd->N[0] = chunkSize;
        grd->N[1] = chunkSize;
        grd->N[2] = chunkSize;

        grd->d[0] = static_cast<double>(m_gridDesc.cellsize);
        grd->d[1] = static_cast<double>(m_gridDesc.cellsize);
        grd->d[2] = static_cast<double>(m_gridDesc.cellsize);

        grd->r0[0] = static_cast<double>(m_gridDesc.origin.x + static_cast<float>(baseX) * m_gridDesc.cellsize);
        grd->r0[1] = static_cast<double>(m_gridDesc.origin.y + static_cast<float>(baseY) * m_gridDesc.cellsize);
        grd->r0[2] = static_cast<double>(m_gridDesc.origin.z + static_cast<float>(baseZ) * m_gridDesc.cellsize);

        grd->nonortho = 0;
        grd->periodic = 0;

        SdfField<float> chunk(chunkSize+1, chunkSize+1, chunkSize+1);
        
        for (int z = 0; z <= chunkSize; ++z)
        {
            for (int y = 0; y <= chunkSize; ++y)
            {
                const float* srcRow = m_grd->rowPtr(baseY + y, baseZ + z) + baseX;
                float* dstRow = chunk.rowPtr(y, z);
                std::memcpy(dstRow, srcRow, static_cast<size_t>(chunkSize+1) * sizeof(float));
            }
        }
        grd->F = reinterpret_cast<GRD_data_type***>(static_cast<float***>(chunk));

        MC33* M = create_MC33(grd);
        surface* S = calculate_isosurface(M, r.isoValue);

        m_chunkData[chunkKey].vertices.reserve(S->nV);
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

            m_chunkData[chunkKey].vertices.push_back(Vertex{ 
                .pos = { p[0], p[1], p[2] }, 
                .normal = { n[0], n[1], n[2] }, 
                .tangent = { t3.x, t3.y, t3.z, 1.0f },
                .color = {1.0f, 1.0f, 1.0f, 1.0f} 
            });
        }

        m_chunkData[chunkKey].indices.reserve(S->nT * 3);
        for (unsigned t = 0; t < S->nT; ++t) 
        {
            m_chunkData[chunkKey].indices.push_back(S->T[t][0]);
            m_chunkData[chunkKey].indices.push_back(S->T[t][1]);
            m_chunkData[chunkKey].indices.push_back(S->T[t][2]);
        }

        free_surface_memory(S);
        free_MC33(M);
    }
    delete grd;
}
