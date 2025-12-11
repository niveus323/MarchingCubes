#include "pch.h"
#include "SceneObject.h"

SceneObject::SceneObject()
{
	m_transformComp = AddComponent<TransformComponent>();
}

void SceneObject::AddChild(std::unique_ptr<SceneObject> child)
{
	if (child)
	{
		child->m_owner = this;
		child->m_scene = m_scene;
		m_children.push_back(std::move(child));
	}
}

void SceneObject::Update(float deltatime)
{
	for (auto& comp : m_components)
		comp->Update(deltatime);
}

void SceneObject::Render()
{
	for (auto& comp : m_components)
		comp->Submit();
}