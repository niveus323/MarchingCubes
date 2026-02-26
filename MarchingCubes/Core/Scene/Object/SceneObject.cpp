#include "pch.h"
#include "SceneObject.h"

BEGIN_REFLECTION(SceneObject, GameObject)
END_REFLECTION()

void SceneObject::Init()
{
    GameObject::Init();
    m_transformComp = AddComponent<TransformComponent>();
}
