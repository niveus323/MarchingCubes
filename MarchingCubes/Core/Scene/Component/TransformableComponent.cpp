#include "pch.h"
#include "TransformableComponent.h"
#include "Core/Scene/Component/TransformComponent.h"
#include "Core/Scene/Object/GameObject.h"

BEGIN_REFLECTION(TransformableComponent, Component)
	REFLECT_REQUIRE_COMPONENT(TransformComponent)
END_REFLECTION()

void TransformableComponent::Init()
{
	Component::Init();
	m_transformCache = GetOwner()->GetComponent<TransformComponent>();
}

TransformComponent* TransformableComponent::GetTransformComp() const
{
	return m_transformCache;
}

