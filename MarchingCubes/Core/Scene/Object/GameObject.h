#pragma once
#include "Core/Scene/Component/Component.h"
#include "Core/Scene/Class/Entity.h"

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


class GameObject : public Entity
{
	REFLECT_GENERATED_BODY(GameObject)
public:
	virtual void Init() {}
	virtual void Destroy();
	virtual void BeginPlay() {}
	virtual void EndPlay() {}
	virtual void Update(float deltatime)
	{
		for (auto& comp : m_components)
		{
			if(comp->IsActive())
				comp->Update(deltatime);
		}
	}
	virtual void Render() {}
	virtual void OnPreSave() {}
	virtual void Serialize(Serializer& ar) override;

	void MarkForDestroy();
	bool IsPendingDestroy() const { return HasAnyFlags(EObjectFlags::PendingKill); }

	template<std::derived_from<Component> T = Component>
	T* AddComponent(EObjectFlags flags = EObjectFlags::None)
	{
		return InternalAddComponent(std::make_shared<T>(), flags);
	}
	
	// 런타임용 Component 생성 함수
	Component* AddComponent(TypeDescriptor* typeDesc, EObjectFlags flags = EObjectFlags::None);

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

	template<std::derived_from<GameObject> T = GameObject>
	T* CreateChild(std::string_view name, EObjectFlags flags = EObjectFlags::None)
	{
		auto newChild = std::make_shared<T>();
		newChild->SetName(std::string(name));
		newChild->m_owner = GetSharedPtr<GameObject>();
		newChild->m_scene = m_scene;
		newChild->SetFlags(flags);
		newChild->Init();
		T* ptr = newChild.get();
		m_children.push_back(std::move(newChild));

		return ptr;
	}

	void AddChild(std::shared_ptr<GameObject> child);

	void RemoveChild(std::shared_ptr<GameObject> child)
	{
		if (!child) return;
		std::shared_ptr<GameObject> childsOwner = child->GetOwner();
		if (childsOwner.get() != this) return;

		child->m_owner.reset();

		std::erase_if(m_children, [&](const std::shared_ptr<GameObject>& ptr) { return ptr == child; });
	}

	void SetActive(bool bActive)
	{
		m_bActive = bActive;
		for (auto& comp : m_components)
		{
			comp->SetActive(bActive);
		}

		for (auto& child : m_children)
		{
			child->SetActive(bActive);
		}
	}
	bool IsActive() const { return m_bActive; }

	std::shared_ptr<GameObject> GetOwner() { return m_owner.lock(); }
	std::shared_ptr<Scene> GetScene() { return m_scene.lock(); }
	void SetScene(std::shared_ptr<Scene> scene) { m_scene = scene; }
	auto& GetComponents() const { return m_components; }
	auto& GetChildren() const { return m_children; }

	void RegisterComponent(std::shared_ptr<Component>&& comp)
	{
		TypeDescriptor* typeDesc = comp->GetType();
		std::string typeName = typeDesc->GetName();

		int sameTypeCount = 0;
		for (const auto& existing : m_components)
		{
			if (existing->GetType() != typeDesc) continue;
			sameTypeCount++;
		}
		std::string candidateName;
		candidateName = typeName + std::to_string(sameTypeCount);
		comp->SetName(candidateName);

		m_components.push_back(std::move(comp));
	}

	void UnregisterComponent(uint64_t uuid)
	{
		for (auto iter = m_components.begin(); iter != m_components.end();)
		{
			if ((*iter)->GetUUID() == uuid)
			{
				(*iter)->Destroy();
				iter = m_components.erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}
private:
	template <typename T>
	T* InternalAddComponent(std::shared_ptr<T> newComponent, EObjectFlags flags)
	{
		newComponent->SetOwner(std::static_pointer_cast<GameObject>(shared_from_this()));
		newComponent->AddFlags(flags);

		T* ptr = newComponent.get();
		RegisterComponent(std::static_pointer_cast<Component>(newComponent));

		ptr->Init();
		return ptr;
	}

protected:
	std::weak_ptr<Scene> m_scene;
	std::weak_ptr<GameObject> m_owner;
	std::vector<std::shared_ptr<Component>> m_components;
	std::vector<std::shared_ptr<GameObject>> m_children;
	bool m_bActive = true;
};

