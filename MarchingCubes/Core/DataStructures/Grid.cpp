#include "pch.h"
#include "Grid.h"

Grid::Grid(int sizeX, int sizeY, int sizeZ, float cellSize, DirectX::XMFLOAT3 origin):
	m_sizeX(sizeX),
	m_sizeY(sizeY),
	m_sizeZ(sizeZ),
	m_cellSize(cellSize),
	m_origin(origin),
	m_density((sizeX + 1) * (sizeY + 1) * (sizeZ + 1), 0.0f)
{
}
