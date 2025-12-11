#include "pch.h"
#include "BaseScene.h"
#include "Core/Scene/SceneObject.h"
#include "Core/Scene/Component/MeshComponent.h"

void BaseScene::Render()
{
	for (const auto rendererComp : m_rendererCache)
	{
		rendererComp->Submit();
	}
}

void BaseScene::AddObject(std::unique_ptr<SceneObject> obj)
{
	obj->SetScene(this);
	m_objects.push_back(std::move(obj));
}
