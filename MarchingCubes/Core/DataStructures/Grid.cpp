#include "pch.h"
#include "Grid.h"

Grid::Grid(const GridParams& params, std::vector<float>&& density) 
	: m_params(params), 
	  m_density(std::move(density))
{
}

float Grid::Sample(int i, int j, int k) const
{
	size_t idx = i + j * m_params.dimX + k * (m_params.dimX * m_params.dimY);
	return m_density[idx];
}

DirectX::XMFLOAT3 Grid::Position(int i, int j, int k) const
{
	return { m_params.origin.x + i * m_params.cellSize.x,
			m_params.origin.y + j * m_params.cellSize.y,
			m_params.origin.z + k * m_params.cellSize.z };
}

#ifdef _DEBUG

void Grid::CreateDebugGrid(ID3D12Device* device)
{

}

void Grid::DrawDebugGrid(ID3D12GraphicsCommandList* cmd)
{

}

#endif
