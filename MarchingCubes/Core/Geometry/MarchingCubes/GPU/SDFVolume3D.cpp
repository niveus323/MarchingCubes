#include "pch.h"
#include "SDFVolume3D.h"

SDFVolume3D::SDFVolume3D(ID3D12Device* device) :
    m_device(device)
{
}

void SDFVolume3D::uploadFromGRD(ID3D12GraphicsCommandList* cmd, const SdfField<float>* grid, std::vector<ComPtr<ID3D12Resource>>& pendingDeletes)
{
    const UINT dimX = static_cast<UINT>(grid->sx());
    const UINT dimY = static_cast<UINT>(grid->sy());
    const UINT dimZ = static_cast<UINT>(grid->sz());

    // 1) density3D 생성/업데이트(+SRV)
    ComPtr<ID3D12Resource> upTex;
    MCUtil::CreateOrUpdateDensity3D(m_device.Get(), cmd, dimX, dimY, dimZ, grid->data(), m_density3D, &upTex);
    NAME_D3D12_OBJECT(m_density3D);
    NAME_D3D12_OBJECT(upTex);

    if (upTex) pendingDeletes.push_back(upTex);
}
