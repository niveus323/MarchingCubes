#include "pch.h"
#include "RendererComponent.h"
#include "Core/Scene/SceneObject.h"
#include "Core/Scene/BaseScene.h"

RendererComponent::RendererComponent(SceneObject* owner) : 
	Component(owner)
{
	if (auto scene = m_owner->GetScene())
	{
		scene->RegisterRenderable(this);
	}
}
