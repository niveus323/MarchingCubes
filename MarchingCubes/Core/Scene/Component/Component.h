#pragma once

//forward declaration
class SceneObject;
class BaseScene;

class Component
{
public:
	Component(SceneObject* owner) : m_owner(owner) {}
	virtual ~Component() = default;

	virtual void Init() {}
	virtual void Update(float deltatime) {}
	virtual void Submit() {}
	BaseScene* GetScene();

protected:
	SceneObject* m_owner;
};

