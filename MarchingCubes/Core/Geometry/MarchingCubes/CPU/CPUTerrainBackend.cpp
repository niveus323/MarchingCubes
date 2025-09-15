#include "pch.h"
#include "CPUTerrainBackend.h"

CPUTerrainBackend::CPUTerrainBackend(const GridDesc& desc, std::shared_ptr<_GRD> grid)
{
	setGridDesc(desc);
	setFieldPtr(grid);
}

void CPUTerrainBackend::setGridDesc(const GridDesc& desc)
{
	m_gridDesc = desc;
}

void CPUTerrainBackend::setFieldPtr(std::shared_ptr<_GRD> grid)
{
	m_grd = std::move(grid);
}

bool CPUTerrainBackend::tryFetch(std::vector<ChunkUpdate>& ups)
{
	// CPU Terrain¿∫ ¥‹¿œ Chunk.
	ups.clear();
	ChunkUpdate up;
	up.empty = m_meshData.indices.empty();
	up.key = { 0,0,0 };
	up.md = std::move(m_meshData);
	ups.push_back(up);

	return !up.empty;
}
