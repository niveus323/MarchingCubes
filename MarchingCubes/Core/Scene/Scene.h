#pragma once
#include "Core/Scene/Object/SceneObject.h"
#include "Core/DataStructures/ShaderTypes.h"
#include "Core/Engine/Subsystem/SceneSubsystem.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Utils/StringUtils.h"
#include <unordered_map>
#include <typeindex>

/* [Scene]
* - Definition : 실행 가능한 장면의 단위
* - LifeTime : Scene Load Requested -> Scene Transition/Exit Requested
* - OwnerShip : Engine
* - Access : Engine::GetCurrentScene() / Engine::GetScene(std::filesystem::path scenePath)
* - Responsibility : 
*	- SceneObject : std::vector로 관리
*   - 
*/
class RendererComponent;
class CameraComponent;
class LightComponent;
class GameMode;
class IUIRenderer;
class Controller;
class Mesh;
class IUIBuilder;

class Scene
{
public:
	Scene();
	virtual ~Scene();

	virtual void Init();
	virtual void InitUI(IUIRenderer* ui);
	virtual void BeginPlay(); // TODO : 프리뷰/게임 시작 연결
	virtual void BeginEditor();
	virtual void EndPlay();
	virtual void EndEditor();
	virtual void OnExit();
	virtual void OnResize(float x, float y, float width, float height);
	virtual void Update(float deltaTime);
	virtual void Render();

	template<std::derived_from<GameObject> T = GameObject, typename... Args>
	T* CreateObject(Args&&... args)
	{
		auto newObj = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* ptr = newObj.get();

		std::string className = GetCleanClassName(typeid(T).name());
		ptr->SetName(className);

		if constexpr (std::derived_from<T, SceneObject>)
		{
			m_sceneObjectsCache.push_back(ptr);
		}
		m_objects.push_back(std::move(newObj));
		ptr->Init();
		return ptr;
	}
	void AddObject(std::unique_ptr<GameObject> obj);

	template<std::derived_from<GameObject> T = GameObject>
	T* FindObject()
	{
		for (auto& object : m_objects)
		{
			if (T* typed = dynamic_cast<T*>(object.get()))
				return typed;
		}
		return nullptr;
	}

	void RegisterLight(LightComponent* light) { m_lightCache.push_back(light); }
	void UnregisterLight(LightComponent* light) { if (!m_lightCache.empty())  std::erase(m_lightCache, light); }

	// Viewport
	D3D12_VIEWPORT GetViewport() const { return m_viewport; }
	D3D12_RECT GetScissorRect() const { return m_scissorRect; }
	float GetViewportX() const { return m_viewport.TopLeftX; }
	float GetViewportY() const { return m_viewport.TopLeftY; }

	CameraConstants GetCameraConstants();
	LightBlobView GetLightBlob();

	template <std::derived_from<ISceneSubsystem> T, typename... Args>
	T* AddSubsystem(Args&&... args)
	{
		auto it = m_sceneSubsystems.find(typeid(T));
		if (it != m_sceneSubsystems.end())
		{
			return static_cast<T*>(it->second.get());
		}

		auto newSystem = std::make_unique<T>(std::forward<Args>(args)...);
		T* rawPtr = newSystem.get();

		EngineCore::RegisterSubsystem<T>(rawPtr);

		m_sceneSubsystems[typeid(T)] = std::move(newSystem);

		return rawPtr;
	}

	template <std::derived_from<ISceneSubsystem> T>
	T* GetSubsystem()
	{
		auto it = m_sceneSubsystems.find(typeid(T));
		if (it != m_sceneSubsystems.end())
			return static_cast<T*>(it->second.get());
		return nullptr;
	}
	CameraComponent* GetMainCamera() { return m_mainCamera; }

	const auto& GetObjects() const { return m_objects; }
protected:
	void ClearSubsystems()
	{
		// 엔진 목록에서 제거
		for (const auto& [type, system] : m_sceneSubsystems)
		{
			EngineCore::UnregisterSubsystem(type);
		}
		m_sceneSubsystems.clear();
	}

	void SetMainCamera(CameraComponent* cameraComp);
	
	GameMode* GetGameMode() { return m_gameMode; }
	void SetGameMode(GameMode* gameMode) { m_gameMode = gameMode; }

private:
	friend class RendererComponent;
	void RegisterRenderable(RendererComponent* rendererComp) { m_rendererCache.push_back(rendererComp); }
	void UnregisterRenderable(RendererComponent* rendererComp) { std::erase_if(m_rendererCache, [rendererComp](const RendererComponent* target) { return target == rendererComp; }); }

	void RenderSceneGizmoUI(IUIBuilder* ui);

protected:
	CameraComponent* m_mainCamera = nullptr;

	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	float m_viewportWidth = 0.0f;
	float m_viewportHeight = 0.0f;
	float m_viewportX = 0.0f;
	float m_viewportY = 0.0f;

	std::vector<std::unique_ptr<Mesh>> m_editorMeshes;

private:
	bool m_isPlaying = false;

	std::vector<std::unique_ptr<GameObject>> m_objects;

	// Cache
	std::vector<SceneObject*> m_sceneObjectsCache;
	std::vector<RendererComponent*> m_rendererCache;
	std::vector<LightComponent*> m_lightCache;
	std::vector<uint8_t> m_lightUploadBuffer;
	GameMode* m_gameMode = nullptr;
	Controller* m_currentController = nullptr;

	std::unordered_map<std::type_index, std::unique_ptr<ISceneSubsystem>> m_sceneSubsystems;

};

