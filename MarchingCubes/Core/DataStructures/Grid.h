#pragma once
#include "Core/Geometry/Mesh.h"

class Grid
{
public:
	Grid(int sizeX, int sizeY, int sizeZ, float cellSize, DirectX::XMFLOAT3 origin);
	
	inline float& operator() (int i, int j, int k)
	{
		return m_density[i + j * (m_sizeX + 1) + k * (m_sizeX + 1) * (m_sizeY + 1)];
	}
	inline const float& operator() (int i, int j, int k) const
	{
		return m_density[i + j * (m_sizeX + 1) + k * (m_sizeX + 1) * (m_sizeY + 1)];
	}

	DirectX::XMFLOAT3 GetWorldPosition(int i, int j, int k) const
	{
		return { m_origin.x + i * m_cellSize, m_origin.y + j * m_cellSize, m_origin.z + k * m_cellSize };
	}

private:
	DirectX::XMFLOAT3 m_origin;
	int m_sizeX, m_sizeY, m_sizeZ;
	float m_cellSize;
	std::vector<float> m_density;
	Mesh mesh;
};

