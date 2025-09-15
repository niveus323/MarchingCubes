#pragma once
#include "Core/Geometry/MarchingCubes/CPU/CPUTerrainBackend.h"
class MC33TerrainBackend : public CPUTerrainBackend
{
public:
	using CPUTerrainBackend::CPUTerrainBackend;

	// CPUTerrainBackend을(를) 통해 상속됨
	void requestBrush(const BrushRequest&) override;
	void requestRemesh(const RemeshRequest&) override;

};

