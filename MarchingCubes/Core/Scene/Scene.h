#pragma once
#include "Core/Scene/Class/Entity.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/DataStructures/ShaderTypes.h"
#include "Core/Engine/Subsystem/SceneSubsystem.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Engine/Reflection.h"
#include "Core/Utils/StringUtils.h"
#include "Core/DataStructures/Data.h"
#include <unordered_map>
#include <unordered_set>
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
class EditorController;
class Mesh;
class IUIBuilder;

class Scene : public Entity
{
public:
	virtual void Init();
	virtual void InitUI(IUIRenderer* ui);
	virtual void BeginPlay(); // TODO : 프리뷰/게임 시작 연결
	virtual void BeginEditor();
	virtual void EndPlay();
	virtual void EndEditor();
	virtual void OnExit(IUIRenderer* ui);
	virtual void OnResize(float width, float height);
	virtual void Update(float deltaTime);
	virtual void Render();
	virtual void Serialize(Serializer& ar) override;

	// --- Object Functions---
	template<std::derived_from<GameObject> T = GameObject>
	T* CreateObject(std::string_view name = "", EObjectFlags flags = EObjectFlags::None)
	{
		auto newObj = std::make_shared<T>();
		newObj->SetFlags(flags);
		newObj->SetScene(std::static_pointer_cast<Scene>(this->shared_from_this()));
		
		// 고유 이름 발급
		std::string baseName = name.empty() ? StringUtils::GetCleanClassName(typeid(T).name()) : std::string(name);
		std::string uniqueName = MakeUniqueName(baseName);
		newObj->SetName(uniqueName);
		m_activeNames.insert(uniqueName);
		
		T* ptr = newObj.get();
		m_uuidMap[newObj->GetUUID()] = newObj.get();
		m_objects.push_back(std::move(newObj));
		ptr->Init();
		return ptr;
	}
	void AddObject(std::shared_ptr<GameObject> obj);

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

	template<std::derived_from<GameObject> T = GameObject>
	std::vector<T*> FindAllObjects()
	{
		std::vector<T*> result;
		for (auto& object : m_objects)
		{
			if (T* typed = dynamic_cast<T*>(object.get()))
			{
				result.push_back(typed);
			}
		}
		return std::move(result);
	}
	GameObject* FindObject(uint64_t uuid);
	// Scene에 배치된 오브젝트의 이름 중복 여부를 체크하여 넘버링 부여
	std::string MakeUniqueName(const std::string& name);

	void RegisterLight(LightComponent* light) { m_lightCache.push_back(light); }
	void UnregisterLight(LightComponent* light) { if (!m_lightCache.empty())  std::erase(m_lightCache, light); }

	//--- Subsystem ---
	template <std::derived_from<ISceneSubsystem> T>
	T* AddSubsystem()
	{
		auto it = m_sceneSubsystems.find(typeid(T));
		if (it != m_sceneSubsystems.end())
		{
			return static_cast<T*>(it->second.get());
		}

		auto newSystem = std::make_unique<T>();
		T* rawPtr = newSystem.get();

		EngineCore::RegisterSubsystem<T>(rawPtr);

		m_sceneSubsystems[typeid(T)] = std::move(newSystem);

		return rawPtr;
	}
	ISceneSubsystem* AddSubsystemByName(const std::string& className);
	void RemoveSubsystem(std::type_index typeIndex);

	template <std::derived_from<ISceneSubsystem> T>
	T* GetSubsystem()
	{
		auto it = m_sceneSubsystems.find(typeid(T));
		if (it != m_sceneSubsystems.end())
			return static_cast<T*>(it->second.get());
		return nullptr;
	}
	const std::unordered_map<std::type_index, std::shared_ptr<ISceneSubsystem>>& GetSubsystems() const { return m_sceneSubsystems; }

	// --- Object Getter & Setter ---
	CameraConstants GetCameraConstants();
	LightBlobView GetLightBlob();
	CameraComponent* GetMainCamera() { return m_mainCamera; }
	void SetMainCamera(CameraComponent* cameraComp);

	GameMode* GetGameMode() { return m_gameMode; }
	void SetGameMode(GameMode* gameMode) { m_gameMode = gameMode; }

	EditorController* GetEditorController() const { return m_editorController; }
	Controller* GetPlayerController(int playerIndex = 0) const;
	const auto& GetObjects() const { return m_objects; }

	// Scene Flags Getter
	bool IsPlaying() const { return m_bPlaying; }

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
private:
	friend class RendererComponent;
	void RegisterRenderable(RendererComponent* rendererComp) { m_rendererCache.push_back(rendererComp); }
	void UnregisterRenderable(RendererComponent* rendererComp) { 
		std::erase_if(m_rendererCache, [rendererComp](const RendererComponent* target) { return target == rendererComp; }); 
	}
protected:
	CameraComponent* m_mainCamera = nullptr;
	float m_viewportWidth = 0.0f;
	float m_viewportHeight = 0.0f;

	bool m_bLoadedFromFile = false; // TODO : 씬 관리는 Data-Driven으로 변경(씬 클래스 상속 불가로)
private:
	bool m_bPlaying = false;

	std::unordered_map<uint64_t, GameObject*> m_uuidMap;
	std::vector<std::shared_ptr<GameObject>> m_objects; //소유용

	// Cache
	std::unordered_set<std::string> m_activeNames;
	std::unordered_map<std::string, uint32_t> m_nameCounters;
	std::vector<RendererComponent*> m_rendererCache;
	std::vector<LightComponent*> m_lightCache;
	std::vector<uint8_t> m_lightUploadBuffer;
	GameMode* m_gameMode = nullptr;
	EditorController* m_editorController = nullptr;

	std::unordered_map<std::type_index, std::shared_ptr<ISceneSubsystem>> m_sceneSubsystems;

	std::vector<size_t> m_uiTokens;
};

