#pragma once

struct GridParams
{
	uint32_t dimX;
	uint32_t dimY;
	uint32_t dimZ;

	DirectX::XMFLOAT3 origin;
	DirectX::XMFLOAT3 cellSize;
};

class Grid
{
public:
	Grid(const GridParams& params, std::vector<float>&& density);

	float Sample(int i, int j, int k) const;
	DirectX::XMFLOAT3 Position(int i, int j, int k) const;
	const GridParams& Params() const { return m_params; }

#ifdef _DEBUG
	void CreateDebugGrid(ID3D12Device* device);
	void DrawDebugGrid(ID3D12GraphicsCommandList* cmd);
#endif // _DEBUG


private:
	GridParams m_params;
	std::vector<float> m_density;
};

