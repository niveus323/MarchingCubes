#include "pch.h"
#include "SDFVolume3D.h"

// --- 내부 도우미: _GRD->F 를 [x][y][z] 선형 배열로 평탄화 ---
static void FlattenGridF_XYZ(const _GRD* grid, std::vector<float>& outLinear)
{
    const UINT X = grid->N[0] + 1;
    const UINT Y = grid->N[1] + 1;
    const UINT Z = grid->N[2] + 1;
    outLinear.resize(size_t(X) * Y * Z);
    auto idx = [X, Y](UINT x, UINT y, UINT z) { return size_t(z) * X * Y + size_t(y) * X + x; };

    for (UINT z = 0; z < Z; ++z)
        for (UINT y = 0; y < Y; ++y)
            for (UINT x = 0; x < X; ++x)
                outLinear[idx(x, y, z)] = grid->F[z][y][x]; // 프로젝트에서 사용하던 z,y,x 순서를 가정
}

SDFVolume3D::SDFVolume3D(ID3D12Device* device) :
    m_device(device)
{
}

void SDFVolume3D::uploadFromGRD(ID3D12GraphicsCommandList* cmd, const _GRD* grid, std::vector<ComPtr<ID3D12Resource>>& pendingDeletes)
{
    const UINT dimX = grid->N[0] + 1;
    const UINT dimY = grid->N[1] + 1;
    const UINT dimZ = grid->N[2] + 1;

    // 1) density3D 생성/업데이트(+SRV)
    ComPtr<ID3D12Resource> upTex;
    std::vector<float> linear;
    FlattenGridF_XYZ(grid, linear);
    MCUtil::CreateOrUpdateDensity3D(m_device.Get(), cmd, dimX, dimY, dimZ, linear.data(), m_density3D, &upTex);
    NAME_D3D12_OBJECT(m_density3D);
    NAME_D3D12_OBJECT(upTex);

    if (upTex) pendingDeletes.push_back(upTex);
}
