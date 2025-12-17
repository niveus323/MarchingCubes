#pragma once
#include "Core/Scene/Component/Component.h"

// Forward Declaration
class Scene;

/* [GameObject]
* - Definition : Component와 GameObject들을 담는 식별 가능한 객체
* - LifeTime : AddObject -> Destroy
* - OwnerShip : Scene
* - Responsibility :
*	- Component : 소유하는 Component를 std::vector로 관리
*	- GameObject : 자식 GameObject를 std::vector로 관리 + 부모 GameObject를 Raw Pointer로 관리
*/

//CRTP 패턴
template<typename Derived>
class GameObjectBase
{
public:
	template<std::derived_from<Component> T, typename... Args>
	T* AddComponent(Args&&...args)
	{
		static_assert(std::derived_from<Derived, GameObjectBase<Derived>>, "CRTP Violation: Derived class must inherit from GameObjectBase<Derived>");
		Derived* derivedThis = static_cast<Derived*>(this);
		auto newComponent = std::make_unique<T>(derivedThis, std::forward<Args>(args)...);
		T* ptr = newComponent.get();
		derivedThis->RegisterComponent(std::move(newComponent));
		ptr->Init();
		return ptr;
	}
};

class GameObject : public GameObjectBase<GameObject>
{
public:
	friend class GameObjectBase<GameObject>;

	GameObject(Scene* scene) : m_scene(scene) {};
	virtual ~GameObject() = default;

	virtual void Init() {}
	virtual void Destroy() {}
	virtual void Update(float deltatime)
	{
		for (auto& comp : m_components)
		{
			if(comp->IsActive())
				comp->Update(deltatime);
		}
	}
	virtual void Render() {}

	template<std::derived_from<Component> T = Component>
	std::vector<T*> GetComponents()
	{
		std::vector<T*> result;
		for (auto& comp : m_components)
			if (T* typed = dynamic_cast<T*>(comp.get()))
				result.push_back(typed);

		for (auto& child : m_children)
		{
			auto childrenResult = child->GetComponents<T>();
			result.insert(result.end(), std::move_iterator(childrenResult.begin()), std::move_iterator(childrenResult.end()));
		}

		return result;
	}

	template<std::derived_from<Component> T = Component>
	T* GetComponent()
	{
		for (auto& comp : m_components)
		{
			if (T* typed = dynamic_cast<T*>(comp.get()))
				return typed;
		}

		return nullptr;
	}

	template<std::derived_from<GameObject> T = GameObject, typename... Args>
	T* CreateChild(Args&&... args)
	{
		auto newChild = std::make_unique<T>(std::forward<Args>(args)...);
		newChild->m_owner = this;
		newChild->m_scene = m_scene;
		T* ptr = newChild.get();
		m_children.push_back(std::move(newChild));

		return ptr;
	}

	void AddChild(std::unique_ptr<GameObject> child)
	{
		if (child)
		{
			child->m_owner = this;
			child->m_scene = m_scene;
			m_children.push_back(std::move(child));
		}
	}


	GameObject* GetOwner() { return m_owner; }
	Scene* GetScene() { return m_scene; }
	void SetScene(Scene* scene) { m_scene = scene; }
	auto& GetComponents() const { return m_components; }
	auto& GetChildren() const { return m_children; }

protected:
	void RegisterComponent(std::unique_ptr<Component>&& comp)
	{
		m_components.push_back(std::move(comp));
	}

protected:
	Scene* m_scene = nullptr;
	GameObject* m_owner = nullptr;
	std::vector<std::unique_ptr<Component>> m_components;
	std::vector<std::unique_ptr<GameObject>> m_children;


};

