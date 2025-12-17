#pragma once
#include "Core/Scene/Object/SceneObject.h"

/* [Scene]
* - Definition : 실행 가능한 장면의 단위
* - LifeTime : Scene Load Requested -> Scene Transition/Exit Requested
* - OwnerShip : Engine
* - Access : Engine::GetCurrentScene() / Engine::GetScene(std::filesystem::path scenePath)
* - Responsibility : 
*	- SceneObject : std::vector로 관리
*   - 
*/
class CameraComponent;
class LightComponent;
class GameMode;
class IUIRenderer;

class Scene
{
public:
	Scene();
	virtual ~Scene();

	virtual void Init();
	virtual void InitUI(IUIRenderer* ui);
	virtual void OnExit();
	virtual void OnResize(float x, float y, float width, float height);
	virtual void Update(float deltaTime);
	virtual void Render();

	template<std::derived_from<GameObject> T = GameObject, typename... Args>
	T* CreateObject(Args&&... args)
	{
		auto newObj = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* ptr = newObj.get();
		if constexpr (std::derived_from<T, SceneObject>)
		{
			m_sceneObjectsCache.push_back(ptr);
		}
		m_objects.push_back(std::move(newObj));
		ptr->Init();
		return ptr;
	}
	void AddObject(std::unique_ptr<GameObject> obj);

	void RegisterLight(LightComponent* light) { m_lightCache.push_back(light); }
	void UnregisterLight(LightComponent* light) { if (!m_lightCache.empty())  std::erase(m_lightCache, light); }

	// Viewport
	D3D12_VIEWPORT GetViewport() const { return m_viewport; }
	D3D12_RECT GetScissorRect() const { return m_scissorRect; }
	float GetViewportX() const { return m_viewport.TopLeftX; }
	float GetViewportY() const { return m_viewport.TopLeftY; }

protected:
	CameraComponent* GetMainCamera() { return m_mainCamera; }
	void SetMainCamera(CameraComponent* cameraComp);
	
	GameMode* GetGameMode() { return m_gameMode; }
	void SetGameMode(GameMode* gameMode) { m_gameMode = gameMode; }

private:
	friend class RendererComponent;
	void RegisterRenderable(RendererComponent* rendererComp) { m_rendererCache.push_back(rendererComp); }
	void UnregisterRenderable(RendererComponent* rendererComp) { std::erase_if(m_rendererCache, [rendererComp](const RendererComponent* target) { return target == rendererComp; }); }

	void RenderSceneGizmoUI();

protected:
	CameraComponent* m_mainCamera = nullptr;

	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	float m_viewportWidth = 0.0f;
	float m_viewportHeight = 0.0f;
	float m_viewportX = 0.0f;
	float m_viewportY = 0.0f;

private:
	std::vector<std::unique_ptr<GameObject>> m_objects;

	// Cache
	std::vector<SceneObject*> m_sceneObjectsCache;
	std::vector<RendererComponent*> m_rendererCache;
	std::vector<LightComponent*> m_lightCache;
	std::vector<uint8_t> m_lightUploadBuffer;
	GameMode* m_gameMode = nullptr;


};

