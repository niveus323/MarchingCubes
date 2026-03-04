#include "pch.h"
#include "SceneObject.h"
#include "Core/Scene/Component/BillboardComponent.h"
#include "Core/Assets/ResourceManager.h"

BEGIN_REFLECTION(SceneObject, GameObject)
END_REFLECTION()

void SceneObject::Init()
{
    GameObject::Init();
    m_transformComp = AddComponent<TransformComponent>();

    // 에디터 뷰포트용 BillboardComponent 추가
    AddComponent<BillboardComponent>(EObjectFlags::Transient | EObjectFlags::EditorOnly);
}
