#pragma once
#include "Core/Engine/Reflection.h"
#include "Core/Scene/Class/Entity.h"

//forward declaration
class GameObject;
class Scene;

class Component : public Entity
{
	REFLECT_GENERATED_BODY(Component)
public:
	virtual void Init() {}
	virtual void Destroy() {}
	virtual void Update(float deltatime) {}
	virtual void Submit() {}
	virtual void Serialize(Serializer& ar) override;

	std::shared_ptr<Scene> GetScene();
	template<std::derived_from<GameObject> T = GameObject>
	T* GetOwner() const { return static_cast<T*>(m_owner.lock().get()); }
	void SetOwner(std::shared_ptr<GameObject> owner) { m_owner = owner; }
	
	bool IsActive() { return m_bActive; }
	void SetActive(bool active) { m_bActive = active; }

protected:
	std::weak_ptr<GameObject> m_owner;
	bool m_bActive = true;
};

