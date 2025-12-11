#pragma once
#include "Core/Scene/SceneObject.h"

/* [BaseScene]
* - LifeTime : Scene Load Requested -> Scene Transition/Exit Requested
* - OwnerShip : Engine
* - Access : Engine::GetCurrentScene() / Engine::GetScene(std::filesystem::path scenePath)
* - Responsibility : 
*	- SceneObject : std::vector·Î °ü¸®
*/
class RenderSystem;

class BaseScene
{
public:
	BaseScene(RenderSystem* rendersystem) : m_rendersystem(rendersystem) 
	{};
	virtual ~BaseScene() = default;

	void Init() {}
	void Update(float deltaTime) {}
	void Render();

	template<std::derived_from<SceneObject> T = SceneObject, typename... Args>
	T* CreateObject(Args&&... args)
	{
		auto newObj = std::make_unique<T>(std::forward<Args>(args)...);
		newObj->SetScene(this);
		T* ptr = newObj.get();
		m_objects.push_back(std::move(newObj));
		return ptr;
	}
	void AddObject(std::unique_ptr<SceneObject> obj);

	RenderSystem* GetRenderSystem() { return m_rendersystem; }

private:

	friend class RendererComponent;
	void RegisterRenderable(RendererComponent* rendererComp) { m_rendererCache.push_back(rendererComp); }
	void UnregisterRenderable(RendererComponent* rendererComp) { std::erase_if(m_rendererCache, [rendererComp](const RendererComponent* target) { return target == rendererComp; }); }

private:
	RenderSystem* m_rendersystem;
	std::vector<std::unique_ptr<SceneObject>> m_objects;
	std::vector<RendererComponent*> m_rendererCache;
};

