#include "pch.h"
#include "Component.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

Scene* Component::GetScene()
{
	return m_owner->GetScene();
}