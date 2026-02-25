#include "pch.h"
#include "SdfDataComponent.h"
#include "Core/Geometry/MarchingCubes/Class/TerrainObject.h"

BEGIN_REFLECTION(SdfDataComponent, Component)
END_REFLECTION()

void SdfDataComponent::SetGridDesc(const GridDesc& setting)
{
	m_settings = setting;

	if (auto* terrainObject = GetOwner<TerrainObject>())
	{
		terrainObject->OnSettingUpdated();
	}
}