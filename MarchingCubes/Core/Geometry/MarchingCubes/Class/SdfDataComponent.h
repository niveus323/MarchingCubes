#pragma once
#include "Core/Scene/Component/Component.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

/* [SdfDataComponent]
* - LifeTime : AddComponent -> RemoveComponent / Object Destroy
* - OwnerShip : Object
* - Access : GameObject::GetComponent
* - Responsibility :
*	- Runtime Rendering : 파일에서 로드되지 않고, 런타임에 절차적으로 생성된 메쉬를 렌더링
*	- Resources : 생성된 Mesh 객체를 ResourceManager를 통하지 않고 직접 소유
*	- Geometry Update : 주입받은 GeometryData를 기반으로 GPU 업로드
*/

class SdfDataComponent : public Component
{
	REFLECT_GENERATED_BODY(SdfDataComponent)
public:
	const GridDesc& GetGridDesc() const { return m_settings; }
	void SetGridDesc(const GridDesc& setting);
	std::shared_ptr<SdfField> GetField() const { return m_data; }
	SdfField* GetFieldPtr() const { return m_data.get(); }
	void SetData(std::shared_ptr<SdfField> field) { m_data = std::move(field); }
private:
	std::shared_ptr<SdfField>	m_data;
	GridDesc					m_settings{};
};

