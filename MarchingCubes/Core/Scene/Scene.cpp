#include "pch.h"
#include "Scene.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Component/LightComponent.h"
#include "Core/Scene/Object/GameMode/GameMode.h"
#include "Core/Scene/Object/Controller/PlayerController.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Object/SpectatorPawn.h"
#include "Core/Geometry/Mesh/Class/Mesh.h"
#include "Core/UI/Renderer/UIRenderer.h"
#include "Core/UI/Builder/UIBuilder.h"
#include <typeindex>

void Scene::Init()
{
    if (!m_gameMode) m_gameMode = CreateObject<GameMode>("GameMode");
    m_lightCache.clear();
}

void Scene::InitUI(IUIRenderer* ui)
{
    if (!m_bPlaying)
    {
        auto uiToken_Controller = ui->AddFrameRenderCallbackToken( [&controller = m_editorController](IUIBuilder* builder) {
                if(controller) controller->RenderUI(builder);
            },
            UI::UICallbackOptions{
                .layer = UI::EUILayer::Editor_Panel, 
                .enabled = true,
                .id = "EditorControllerUI"
            }
        );
        m_uiTokens.push_back(uiToken_Controller);
    }
}

void Scene::BeginPlay()
{
    m_bPlaying = true;
    for (auto& obj : m_objects)
    {
        obj->BeginPlay();
    }
}

void Scene::BeginEditor()
{
    m_editorController = CreateObject<EditorController>("EditorController", EObjectFlags::EditorOnly);
    auto spectator = CreateObject<SpectatorPawn>("SpectatorPawn", EObjectFlags::EditorOnly);
    m_editorController->Possess(spectator);
    SetMainCamera(spectator->GetComponent<CameraComponent>());
    Pawn* pawn = m_editorController->GetPawn();
    pawn->SetPosition({ 0.0f, 0.0f, -120.0f }); //TODO : 에디터 설정으로 변경
}

void Scene::EndPlay()
{
    m_bPlaying = false;

    for (auto& obj : m_objects)
    {
        obj->EndPlay();
    }
    // GameMode, PlayerController 제거 및 동적 생성된 게임 용 오브젝트 제거
    if (m_gameMode)
    {
        if (auto pc = m_gameMode->GetController())
        {
            if (auto pawn = pc->GetPawn()) pawn->MarkForDestroy();
            pc->MarkForDestroy();
        }
    }
}

void Scene::EndEditor()
{
    // EditorController, SpectatorPawn, Gizmo 등 에디터 용 오브젝트 제거
    if (m_editorController)
    {
        if (auto pawn = m_editorController->GetPawn()) pawn->MarkForDestroy();
        m_editorController->MarkForDestroy();
        m_editorController = nullptr;
    }
}

void Scene::OnExit(IUIRenderer* ui)
{
    if (ui)
    {
        for (auto& token : m_uiTokens)
        {
            ui->RemoveFrameRenderCallback(token);
        }
        m_uiTokens.clear();
    }

    if (m_bPlaying) EndPlay();
    else EndEditor();

    ClearSubsystems();

    m_rendererCache.clear();
    m_lightCache.clear();
    m_objects.clear();
}

void Scene::OnResize(float width, float height)
{
    m_viewportWidth = width;
    m_viewportHeight = height;
    if (m_mainCamera)
    {
        m_mainCamera->SetViewport(m_viewportWidth, m_viewportHeight);
    }
}

void Scene::Update(float deltaTime)
{
    for (auto& [type, subsys] : m_sceneSubsystems)
    {
        subsys->Update(deltaTime);
    }

    for (auto& obj : m_objects)
    {
        if (obj->IsPendingDestroy()) continue;
        obj->Update(deltaTime);
    }

    std::erase_if(m_objects, [this](const std::shared_ptr<GameObject>& obj) {
        if (obj->IsPendingDestroy())
        {
            // 부모에 대한 참조 제거
            if (auto parent = obj->GetOwner())
            {
                if (!parent->IsPendingDestroy()) parent->RemoveChild(obj);
            }

            obj->Destroy();
            m_uuidMap.erase(obj->GetUUID());
            if (m_editorController && m_editorController->GetSelectedObject() == obj.get())
            {
                m_editorController->SelectObject(nullptr);
            }
            return true;
        }
        return false;
    });
}

void Scene::Render()
{	
	for (const auto rendererComp : m_rendererCache)
	{
        // 게임 실행 중에는 EditorOnly인 컴포넌트들을 패스
        if (rendererComp->HasAnyFlags(EObjectFlags::EditorOnly) && m_bPlaying) continue; 
        if (rendererComp->IsActive()) rendererComp->Submit();
	}
}

void Scene::Serialize(Serializer& ar)
{
    Entity::Serialize(ar);
 
    // SceneSubsystem 우선 직렬화
    size_t subsysCount = m_sceneSubsystems.size();
    ar.BeginArray("Subsystem", subsysCount);
    if (ar.IsSaving())
    {
        for (auto& [key, subsys] : m_sceneSubsystems)
        {
            ar.BeginObject("SceneSubsystem");
            std::string typeName = subsys->GetType()->GetName();
            ar.Serialize("Type", typeName);
            ar.EndObject();
        }
    }
    else
    {
        for (size_t i = 0; i < subsysCount; ++i)
        {
            ar.BeginObject("SceneSubsystem");
            std::string className;
            ar.Serialize("Type", className);
            if (TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(className))
            {
                if (ISceneSubsystem* subsysPtr = static_cast<ISceneSubsystem*>(typeDesc->CreateInstance()))
                {
                    std::unique_ptr<ISceneSubsystem> newSubsys(subsysPtr);
                    EngineCore::RegisterSubsystem(typeid(*subsysPtr), subsysPtr);
                    m_sceneSubsystems[std::type_index(typeid(*subsysPtr))] = std::move(newSubsys);
                }
                else
                {
                    Log::Print("Invalid SceneSubsystem Type : %s", className.c_str());
                }
            }
            else
            {
                Log::Print("Unknown SceneSubsystem Type: %s", className.c_str());
            }
            ar.EndObject();
        }
    }

    ar.EndArray();

    size_t objCount = GetObjects().size();
    ar.BeginArray("Objects", objCount);

    if (ar.IsSaving())
    {
        // 저장 전 처리가 필요한 오브젝트 처리
        for (const auto& obj : GetObjects())
        {
            obj->OnPreSave();
        }

        for (const auto& obj : GetObjects())
        {
            if (obj->IsTransient()) continue; 

            ar.BeginObject("Object");
            std::string className = obj->GetType()->GetName();
            ar.Serialize("Type", className);
            obj->Serialize(ar);
            ar.EndObject();
        }
    }
    else // Loading
    {
        for (size_t i = 0; i < objCount; ++i)
        {
            ar.BeginObject("Object");
            std::string className;
            ar.Serialize("Type", className);
            if (TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(className))
            {
                if (GameObject* newObj = static_cast<GameObject*>(typeDesc->CreateInstance()))
                {
                    std::shared_ptr<GameObject> newObject(newObj);
                    newObject->SetScene(this->GetSharedPtr<Scene>());
                    newObject->Init();
                    newObject->Serialize(ar);
                    AddObject(std::move(newObject));
                }
                else
                {
                    Log::Print("Invalid GameObject Type : %s", className.c_str());
                }
            }
            else
            {
                Log::Print("Unknown Object Type: %s", className.c_str());
            }

            ar.EndObject();
        }

        // GameMode 로드 실패 시 디폴트 GameMode 생성
        m_gameMode = FindObject<GameMode>();
        if (!m_gameMode) m_gameMode = CreateObject<GameMode>("GameMode");
    }
    ar.EndArray();

    m_bLoadedFromFile = true;
}

void Scene::AddObject(std::shared_ptr<GameObject> obj)
{
	obj->SetScene(this->GetSharedPtr<Scene>());
    m_uuidMap[obj->GetUUID()] = obj.get();
	m_objects.push_back(std::move(obj));
}

GameObject* Scene::FindObject(uint64_t uuid)
{
    auto it = m_uuidMap.find(uuid);
    if (it != m_uuidMap.end()) return it->second;
    return nullptr;
}

std::string Scene::MakeUniqueName(const std::string& name)
{
    uint32_t& count = m_nameCounters[name];
    std::string candidateName = std::string(name);
    while (m_activeNames.find(candidateName) != m_activeNames.end())
    {
        candidateName = name + std::to_string(count);
        count++;
    }
    return candidateName;
}

ISceneSubsystem* Scene::AddSubsystemByName(const std::string& className)
{
    if (TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(className))
    {
        if (ISceneSubsystem* subsysPtr = static_cast<ISceneSubsystem*>(typeDesc->CreateInstance()))
        {
            std::unique_ptr<ISceneSubsystem> newSubsys(subsysPtr);
            EngineCore::RegisterSubsystem(typeid(*subsysPtr), subsysPtr);

            // 기존 맵에 추가
            m_sceneSubsystems[std::type_index(typeid(*subsysPtr))] = std::move(newSubsys);
            return subsysPtr;
        }
        else
        {
            Log::Print("Invalid SceneSubsystem Type : %s", className.c_str());
        }
    }
    else
    {
        Log::Print("Unknown SceneSubsystem Type: %s", className.c_str());
    }
    return nullptr;
}

void Scene::RemoveSubsystem(std::type_index typeIndex)
{
    auto it = m_sceneSubsystems.find(typeIndex);
    if (it != m_sceneSubsystems.end())
    {
        EngineCore::UnregisterSubsystem(typeIndex);
        m_sceneSubsystems.erase(it);
    }
}

CameraConstants Scene::GetCameraConstants()
{
    return GetMainCamera()->GetCameraConstants();
}

LightBlobView Scene::GetLightBlob()
{
    uint32_t lightCount = (uint32_t)m_lightCache.size();
    size_t headerSize = sizeof(LightConstantsHeader);
    size_t dataSize = sizeof(Light) * lightCount;
    size_t totalBytes = headerSize + dataSize;

    if (m_lightUploadBuffer.size() < totalBytes) m_lightUploadBuffer.resize(totalBytes);

    LightConstantsHeader header{ .lightCounts = lightCount };
    memcpy(m_lightUploadBuffer.data(), &header, headerSize);

    Light* lightDataPtr = reinterpret_cast<Light*>(m_lightUploadBuffer.data() + headerSize);
    for (size_t i = 0; i < lightCount; ++i) 
        lightDataPtr[i] = m_lightCache[i]->GetLightInfo();

    return LightBlobView{
        .data = m_lightUploadBuffer.data(),
        .size = (uint32_t)totalBytes
    };
}

void Scene::SetMainCamera(CameraComponent* cameraComp)
{
    m_mainCamera = cameraComp;
    if (m_mainCamera && m_viewportWidth > 0 && m_viewportHeight > 0)
    {
        m_mainCamera->SetViewport(m_viewportWidth, m_viewportHeight);
    }
}

Controller* Scene::GetPlayerController(int playerIndex) const
{
    if (!m_gameMode) return nullptr;
    return m_gameMode->GetController(playerIndex);
}
