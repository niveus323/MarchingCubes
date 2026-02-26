#include "pch.h"
#include "Component.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(Component, Entity)
END_REFLECTION()

void Component::Serialize(Serializer& ar)
{
	Entity::Serialize(ar);
	ar.Serialize("Active", m_bActive);
}

std::shared_ptr<Scene> Component::GetScene()
{
	return (m_owner.lock())->GetScene();
}