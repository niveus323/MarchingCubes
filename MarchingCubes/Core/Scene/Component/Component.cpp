#include "pch.h"
#include "Component.h"
#include "Core/Scene/SceneObject.h"
#include "Core/Scene/BaseScene.h"

BaseScene* Component::GetScene()
{
	return m_owner->GetScene();
}