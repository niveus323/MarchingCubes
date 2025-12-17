#include "pch.h"
#include "RendererComponent.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Scene.h"

RendererComponent::RendererComponent(SceneObject* owner) : 
	Component(owner)
{
}

void RendererComponent::Init()
{
	if (auto scene = GetOwner()->GetScene())
	{
		scene->RegisterRenderable(this);
	}
}

void RendererComponent::Destroy()
{
	if (auto scene = GetOwner()->GetScene())
	{
		scene->UnregisterRenderable(this);
	}

}
