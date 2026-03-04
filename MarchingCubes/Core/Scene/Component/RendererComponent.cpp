#include "pch.h"
#include "RendererComponent.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(RendererComponent, TransformableComponent)
END_REFLECTION()

void RendererComponent::Init()
{
	TransformableComponent::Init();
	if (auto scene = GetOwner()->GetScene())
	{
		scene->RegisterRenderable(this);
	}
}

void RendererComponent::Destroy()
{
	TransformableComponent::Destroy();
	if (auto scene = GetOwner()->GetScene())
	{
		scene->UnregisterRenderable(this);
	}

}
