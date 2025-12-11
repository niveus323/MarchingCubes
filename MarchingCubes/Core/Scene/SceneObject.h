#pragma once
#include "Core/Scene/Component/Component.h"
#include "Core/Scene/Component/TransformComponent.h"

/* [SceneObject]
* - LifeTime : Scene Load -> Scene UnLoad
* - OwnerShip : Scene
* - Access : Scene::GetSceneObject
* - Responsibility :
*	- Component : 소유하는 Component를 std::vector로 관리
*	- SceneObject : 자식 SceneObject를 std::vector로 관리 + 부모 SceneObject를 Raw Pointer로 관리
*	- TransformComponent : TransformComponent를 캐싱하여 관리
*	- Scene : 해당 오브젝트를 소유중인 Scene 참조
*/
class SceneObject
{
public:
	SceneObject();
	~SceneObject() = default;

	template<std::derived_from<Component> T, typename... Args>
	T* AddComponent(Args&&...args)
	{
		auto newComponent = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* ptr = newComponent.get();
		m_components.push_back(std::move(newComponent));
		return ptr;
	}

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

	template<std::derived_from<SceneObject> T = SceneObject, typename... Args>
	T* CreateChild(Args&&... args)
	{
		auto newChild = std::make_unique<T>(std::forward<Args>(args)...);
		newChild->m_owner = this;
		newChild->m_scene = m_scene;
		T* ptr = newChild.get();
		m_children.push_back(std::move(newChild));

		return ptr;
	}

	void AddChild(std::unique_ptr<SceneObject> child);
	void Update(float deltatime);
	void Render();

	SceneObject* GetOwner() { return m_owner; }
	BaseScene* GetScene() { return m_scene; }
	void SetScene(BaseScene* scene) { m_scene = scene; }
	TransformComponent* GetTransformComponent() { return m_transformComp; }

	DirectX::XMMATRIX GetTransformMatrix() { return m_transformComp->GetWorldMatrix(); }
	DirectX::XMFLOAT4X4 GetTransform()
	{
		DirectX::XMFLOAT4X4 worldMatrix;
		DirectX::XMStoreFloat4x4(&worldMatrix, GetTransformMatrix());
		return worldMatrix;
	}
	void SetPosition(const DirectX::XMFLOAT3& pos) { m_transformComp->SetPosition(pos); }
	void SetRotation(const DirectX::XMFLOAT3& rot) { m_transformComp->SetRotation(rot); }
	void SetScale(const DirectX::XMFLOAT3& scale) { m_transformComp->SetScale(scale); }
	void SetTransform(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot, const DirectX::XMFLOAT3& scale) { m_transformComp->SetTransform(pos, rot, scale); }

private:
	BaseScene* m_scene = nullptr;
	SceneObject* m_owner = nullptr;
	TransformComponent* m_transformComp;
	std::vector<std::unique_ptr<Component>> m_components;
	std::vector<std::unique_ptr<SceneObject>> m_children;
	
};