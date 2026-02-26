#include "pch.h"
#include "CPUTerrainBackend.h"
#include <MC33_c/marching_cubes_33.h>
#include <cmath>
#include <chrono>

CPUTerrainBackend::CPUTerrainBackend(ID3D12Device* device)
{
}

CPUTerrainBackend::~CPUTerrainBackend()
{
}

void CPUTerrainBackend::PushRequest(BuildRequest&& request)
{
    CleanupFinishedTasks();

    m_runningTasks.push_back(std::async(std::launch::async, [this, req = std::move(request)]() mutable {
        BuildResult result{
            .key = req.key,
            .ptr = req.ptr
        };
        if (!req.ptr.expired())
        {
            ProcessMarchingCubes(req, result);
        }

        {
            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_results.emplace_back(std::move(result));
        }
    }));
}

bool CPUTerrainBackend::TryFetch(std::vector<BuildResult>& OutResults)
{
    CleanupFinishedTasks();

    std::lock_guard<std::mutex> lock(m_resultMutex);
    if (m_results.empty()) return false;

    OutResults.reserve(OutResults.size() + m_results.size());
    std::move(m_results.begin(), m_results.end(), std::back_inserter(OutResults));

    m_results.clear();
    return true;
}

bool CPUTerrainBackend::HasRequests() const
{
    std::lock_guard<std::mutex> lock(m_resultMutex);
    return !m_runningTasks.empty() || !m_results.empty();
}

void CPUTerrainBackend::ProcessMarchingCubes(BuildRequest& request, BuildResult& result)
{
    if (!request.fieldData) return;

    auto& desc = request.setting;
    auto& chunkKey = request.key;

    _GRD localGrd{};
    int cellsPerChunk = desc.cellsPerChunk;
    const int baseX = chunkKey.x * cellsPerChunk;
    const int baseY = chunkKey.y * cellsPerChunk;
    const int baseZ = chunkKey.z * cellsPerChunk;

    localGrd.N[0] = cellsPerChunk;
    localGrd.N[1] = cellsPerChunk;
    localGrd.N[2] = cellsPerChunk;

    localGrd.d[0] = static_cast<double>(desc.cellsize);
    localGrd.d[1] = static_cast<double>(desc.cellsize);
    localGrd.d[2] = static_cast<double>(desc.cellsize);

    localGrd.r0[0] = static_cast<double>(desc.origin.x + static_cast<float>(baseX) * desc.cellsize);
    localGrd.r0[1] = static_cast<double>(desc.origin.y + static_cast<float>(baseY) * desc.cellsize);
    localGrd.r0[2] = static_cast<double>(desc.origin.z + static_cast<float>(baseZ) * desc.cellsize);

    localGrd.nonortho = 0;
    localGrd.periodic = 0;

    SdfField chunk(cellsPerChunk + 1, cellsPerChunk + 1, cellsPerChunk + 1);
    for (int z = 0; z <= cellsPerChunk; ++z)
    {
        for (int y = 0; y <= cellsPerChunk; ++y)
        {
            // 읽기는 Thread-Safe (m_grd가 const라면)
            const float* srcRow = request.fieldData->rowPtr(baseY + y, baseZ + z) + baseX;
            float* dstRow = chunk.rowPtr(y, z);
            std::memcpy(dstRow, srcRow, static_cast<size_t>(cellsPerChunk + 1) * sizeof(float));
        }
    }
    localGrd.F = reinterpret_cast<GRD_data_type***>(static_cast<float***>(chunk));

    MC33* M = create_MC33(&localGrd);
    surface* S = calculate_isosurface(M, desc.isoValue);

    // 현재 nV, nT 안나옴.
    result.vertices.reserve(S->nV);
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

        result.vertices.push_back(Vertex{
            .pos = { p[0], p[1], p[2] },
            .normal = { n[0], n[1], n[2] },
            .tangent = { t3.x, t3.y, t3.z, 1.0f }
            });
    }

    result.indices.reserve(S->nT * 3);
    for (unsigned t = 0; t < S->nT; ++t)
    {
        result.indices.push_back(S->T[t][0]);
        result.indices.push_back(S->T[t][1]);
        result.indices.push_back(S->T[t][2]);
    }

    free_surface_memory(S);
    free_MC33(M);
}

void CPUTerrainBackend::CleanupFinishedTasks()
{
    std::erase_if(m_runningTasks, [](const std::future<void>& f) {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
}
