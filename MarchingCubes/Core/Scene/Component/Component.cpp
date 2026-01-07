#include "pch.h"
#include "Component.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION_ROOT(Component)
END_REFLECTION()

Scene* Component::GetScene()
{
	return m_owner->GetScene();
}