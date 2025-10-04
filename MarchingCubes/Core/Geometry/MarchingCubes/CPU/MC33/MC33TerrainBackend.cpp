#include "pch.h"
#include "MC33TerrainBackend.h"

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
