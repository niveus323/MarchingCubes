#pragma once

//forward declaration
class GameObject;
class Scene;

class Component
{
public:
	Component(GameObject* owner) : m_owner(owner) {}
	virtual ~Component() = default;

	virtual void Init() {}
	virtual void Destroy() {}
	virtual void Update(float deltatime) {}
	virtual void Submit() {}
	Scene* GetScene();
	template<std::derived_from<GameObject> T = GameObject>
	T* GetOwner() const { return static_cast<T*>(m_owner); }
	
	bool IsActive() { return m_bActive; }
	void SetActive(bool active) { m_bActive = active; }

protected:
	GameObject* m_owner;
	bool m_bActive = true;
};

