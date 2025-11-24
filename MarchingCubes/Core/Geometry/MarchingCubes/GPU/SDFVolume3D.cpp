#include "pch.h"
#include "SDFVolume3D.h"
#include "Core/Rendering/UploadContext.h"

SDFVolume3D::SDFVolume3D(ID3D12Device* device, UploadContext* uploadContext) :
	m_device(device),
	m_uploadContext(uploadContext)
{
}

void SDFVolume3D::uploadFromGRD(ID3D12GraphicsCommandList* cmd, const SdfField<float>* grid)
{
	const uint32_t dimX = static_cast<uint32_t>(grid->sx());
	const uint32_t dimY = static_cast<uint32_t>(grid->sy());
	const uint32_t dimZ = static_cast<uint32_t>(grid->sz());

	EnsureDensityTex(dimX, dimY, dimZ);

	D3D12_SUBRESOURCE_DATA s{};
	s.pData = grid->data();
	s.RowPitch = sizeof(float) * dimX;
	s.SlicePitch = s.RowPitch * dimY;
	std::vector<D3D12_SUBRESOURCE_DATA> subs{ s };
	m_uploadContext->UploadTexture(cmd, m_density3D.Get(), subs, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, "SDFVolume3D");

}

void SDFVolume3D::EnsureDensityTex(const uint32_t dimX, const uint32_t dimY, const uint32_t dimZ)
{
	if (m_density3D) 
	{
		auto desc = m_density3D->GetDesc();
		if (desc.Width == dimX && desc.Height == dimY && desc.DepthOrArraySize == dimZ && desc.Format == DXGI_FORMAT_R32_FLOAT) return;
		
		m_density3D.Reset();
	}
	
	D3D12_RESOURCE_DESC texDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D,
		.Width = dimX,
		.Height = dimY,
		.DepthOrArraySize = static_cast<UINT16>(dimZ),
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R32_FLOAT,
		.SampleDesc{.Count = 1},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	};

	CD3DX12_HEAP_PROPERTIES hpDef(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(m_device->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(m_density3D.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_density3D);
}
