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
#include "Core/UI/UIRenderer.h"
#include "Core/UI/Builder/UIBuilder.h"
#include <typeindex>

Scene::Scene()
{
    m_lightCache.clear();
}

Scene::~Scene()
{
}

void Scene::Init()
{
}

void Scene::InitUI(IUIRenderer* ui)
{
    if (!m_isPlaying)
    {
#ifdef _DEBUG
        auto uiToken_Gizmo = ui->AddFrameRenderCallbackToken(std::bind(&Scene::RenderSceneGizmoUI, this, std::placeholders::_1), UI::UICallbackOptions{
            .layer = UI::EUILayer::Editor_Background,
            .rateHz = 0,
            .enabled = true,
            .id = "SceneGizmo"
            });
        m_uiTokens.push_back(uiToken_Gizmo);
#endif // _DEBUG
        auto uiToken_Controller = ui->AddFrameRenderCallbackToken( [this](IUIBuilder* builder) {
                if (auto editorPC = dynamic_cast<EditorController*>(this->m_currentController))
                {
                    editorPC->RenderUI(builder);
                }
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
    m_isPlaying = true;
    // 디폴트로 GameMode 생성
    m_gameMode = CreateObject<GameMode>("GameMode");
    m_currentController = m_gameMode->GetController<PlayerController>();

    // 게임용 메인 카메라 찾기
    if (!m_mainCamera)
    {
        // 찾기 편하기 위해 CameraObject를 만드는게 나을 것 같다.
        // Component로 찾으려고 하면 Object 전체 순회 + Component 순회가 발생.
    }
}

void Scene::BeginEditor()
{
    auto editorPC = CreateObject<EditorController>("EditorController");
    auto spectator = CreateObject<SpectatorPawn>("SpectatorPawn");
    editorPC->Possess(spectator);
    SetMainCamera(spectator->GetComponent<CameraComponent>());
    m_currentController = editorPC;
    Pawn* pawn = m_currentController->GetPawn();
    pawn->SetPosition({ 0.0f, 0.0f, -120.0f });
}

void Scene::EndPlay()
{
    // TODO : GameMode, PlayerController 제거 및 동적 생성된 게임 용 오브젝트 제거
    m_isPlaying = false;
}

void Scene::EndEditor()
{
    // TODO : EditorController, SpectatorPawn, Gizmo 등 에디터 용 오브젝트 제거

    //m_editorMeshes.clear();
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

    if (m_isPlaying) EndPlay();
    else EndEditor();

    ClearSubsystems();

    m_rendererCache.clear();
    m_lightCache.clear();
    m_objects.clear();
}

void Scene::OnResize(float x, float y, float width, float height)
{
    m_viewport.TopLeftX = x;
    m_viewport.TopLeftY = y;
    m_viewport.Width = width;
    m_viewport.Height = height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left = static_cast<LONG>(x);
    m_scissorRect.top = static_cast<LONG>(y);
    m_scissorRect.right = static_cast<LONG>(x + width);
    m_scissorRect.bottom = static_cast<LONG>(y + height);

    m_viewportX = x;
    m_viewportY = y;
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
        obj->Update(deltaTime);
    }
}

void Scene::Render()
{	
	for (const auto rendererComp : m_rendererCache)
	{
        if(rendererComp->IsActive()) rendererComp->Submit();
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
    {
        lightDataPtr[i] = m_lightCache[i]->GetLightInfo();
    }

    return LightBlobView{
        .data = m_lightUploadBuffer.data(),
        .size = (uint32_t)totalBytes
    };
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

void Scene::SetMainCamera(CameraComponent* cameraComp)
{
    m_mainCamera = cameraComp;
    if (m_mainCamera && m_viewportWidth > 0 && m_viewportHeight > 0)
    {
        m_mainCamera->SetViewport(static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight));
    }
}

void Scene::RenderSceneGizmoUI(IUIBuilder* ui)
{
    // 씬 뷰포트 좌측 하단에 붙어있는 기즈모
    UI::Vector<float, 2> mainViewportPos = ui->GetMainViewportPos();
    float windowScreenX = mainViewportPos.x;
    float windowScreenY = mainViewportPos.y;
    float gizmoSize = 100.0f;
    float gizmoX = (windowScreenX + GetViewportX());
    float gizmoY = (windowScreenY + GetViewportY()) + m_viewportHeight - gizmoSize;
    if (ui->BeginOverlay("Gizmo", { gizmoX, gizmoY }, { gizmoSize, gizmoSize }))
    {
        // 중심점 계산
        UI::Vector<float, 2> center = ui->GetCursorScreenPos();
        center.x += 50.0f;
        center.y += 50.0f;
        float radius = 40.0f;

        struct Axis {
            XMVECTOR direction;
            UI::Color color;
            const char* label;
            float zDepth;
        };

        std::vector<Axis> axes = {
            { XMVectorSet(1, 0, 0, 0), {1.0f, 0.2f, 0.2f, 1.0f}, "X", 0.0f },
            { XMVectorSet(0, 1, 0, 0), {0.2f, 1.0f, 0.2f, 1.0f}, "Y", 0.0f },
            { XMVectorSet(0, 0, 1, 0), {0.2f, 0.2f, 1.0f, 1.0f}, "Z", 0.0f }
        };

        // 회전 계산
        if (m_mainCamera)
        {
            XMMATRIX viewMat = m_mainCamera->GetViewMatrix();
            for (auto& axis : axes)
            {
                XMVECTOR viewDir = XMVector3TransformNormal(axis.direction, viewMat);
                axis.direction = viewDir;
                axis.zDepth = XMVectorGetZ(viewDir);
            }
        }

        // Z-Sort (뒤에 있는 축부터 그리기 위해)
        std::sort(axes.begin(), axes.end(), [](const Axis& a, const Axis& b) {
            return a.zDepth < b.zDepth;
        });

        for (const auto& axis : axes)
        {
            float x = XMVectorGetX(axis.direction);
            float y = XMVectorGetY(axis.direction);
            UI::Vector<float, 2> endPos = { center.x + x * radius, center.y - y * radius };
            ui->DrawLine(center, endPos, axis.color, 3.0f); // 라인 그리기
            ui->DrawCircleFilled(endPos, 7.0f, axis.color); // 끝점 원 그리기

            // 텍스트 라벨 그리기 (중앙 정렬)
            UI::Vector<float, 2> textSize = ui->CalcTextSize(axis.label);
            UI::Vector<float, 2> textPos = {
                endPos.x - textSize.x * 0.5f,
                endPos.y - textSize.y * 0.5f
            };
            ui->DrawTextAt(textPos, { 1.0f, 1.0f, 1.0f, 1.0f }, axis.label);
        }
        ui->DrawCircleFilled(center, 4.0f, { 1.0f, 1.0f, 1.0f, 1.0f }); // Pivot
    }
    ui->EndOverlay(); // 오버레이 종료
}
