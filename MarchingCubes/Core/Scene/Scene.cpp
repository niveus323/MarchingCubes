#include "pch.h"
#include "Scene.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Component/LightComponent.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Scene/Object/GameMode.h"
#include "Core/UI/ImGUIRenderer.h"

Scene::Scene()
{
    m_lightCache.clear();
}

Scene::~Scene()
{
}

void Scene::Init()
{
    // 디폴트로 GameMode 생성
    m_gameMode = CreateObject<GameMode>();
}

void Scene::InitUI(IUIRenderer* ui)
{
#ifdef _DEBUG
    ui->AddFrameRenderCallbackToken(std::bind(&Scene::RenderSceneGizmoUI, this), UI::UICallbackOptions{
        .priority = 100,
        .rateHz = 0,
        .enabled = true,
        .id = "SceneGizmo"
        });
#endif // _DEBUG
}

void Scene::OnExit()
{
    m_sceneObjectsCache.clear();
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
    for (auto& obj : m_objects)
    {
        obj->Update(deltaTime);
    }
}

void Scene::Render()
{
    if (auto renderSystem = EngineCore::GetRenderSystem())
    {
        CameraConstants sceneViewData = GetMainCamera()->GetCameraConstants();

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

        LightBlobView lightBlob;
        lightBlob.data = m_lightUploadBuffer.data();
        lightBlob.size = (uint32_t)totalBytes;
        renderSystem->PrepareRender(EngineCore::GetUploadContext(), EngineCore::GetDescriptorAllocator(), sceneViewData, lightBlob, EngineCore::GetFrameIndex());
    }
	
	for (const auto rendererComp : m_rendererCache)
	{
		rendererComp->Submit();
	}
}

void Scene::AddObject(std::unique_ptr<GameObject> obj)
{
	obj->SetScene(this);
	m_objects.push_back(std::move(obj));
}

void Scene::SetMainCamera(CameraComponent* cameraComp)
{
    m_mainCamera = cameraComp;
    if (m_mainCamera && m_viewportWidth > 0 && m_viewportHeight > 0)
    {
        m_mainCamera->SetViewport(static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight));
    }
}

void Scene::RenderSceneGizmoUI()
{
#ifdef _DEBUG
    // 씬 뷰포트 좌측 하단에 붙어있는 기즈모
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    float windowScreenX = mainViewport->Pos.x;
    float windowScreenY = mainViewport->Pos.y;

    float gizmoSize = 100.0f;
    float gizmoX = (windowScreenX + GetViewportX());
    float gizmoY = (windowScreenY + GetViewportY()) + m_viewportHeight - gizmoSize;
    ImGui::SetNextWindowPos(ImVec2(gizmoX, gizmoY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(gizmoSize, gizmoSize));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Gizmo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 50.0f;
    center.y += 50.0f;
    float radius = 40.0f;

    struct Axis {
        XMVECTOR direction;
        ImU32 color;
        const char* label;
        float zDepth;
    };

    std::vector<Axis> axes = {
        { XMVectorSet(1, 0, 0, 0), IM_COL32(255, 50, 50, 255), "X", 0.0f },
        { XMVectorSet(0, 1, 0, 0), IM_COL32(50, 255, 50, 255), "Y", 0.0f },
        { XMVectorSet(0, 0, 1, 0), IM_COL32(50, 50, 255, 255), "Z", 0.0f }
    };

    XMMATRIX viewMat = m_mainCamera->GetViewMatrix();
    for (auto& axis : axes)
    {
        XMVECTOR viewDir = XMVector3TransformNormal(axis.direction, viewMat);

        axis.direction = viewDir;
        axis.zDepth = XMVectorGetZ(viewDir);
    }

    std::sort(axes.begin(), axes.end(), [](const Axis& a, const Axis& b) {
        return a.zDepth < b.zDepth;
        });

    for (const auto& axis : axes)
    {
        float x = XMVectorGetX(axis.direction);
        float y = XMVectorGetY(axis.direction);

        ImVec2 endPos = ImVec2(center.x + x * radius, center.y - y * radius);
        drawList->AddLine(center, endPos, axis.color, 3.0f);
        drawList->AddCircleFilled(endPos, 7.0f, axis.color);

        ImVec2 textSize = ImGui::CalcTextSize(axis.label);
        drawList->AddText(ImVec2(endPos.x - textSize.x * 0.5f, endPos.y - textSize.y * 0.5f), IM_COL32(255, 255, 255, 255), axis.label);
    }

    // Pivot
    drawList->AddCircleFilled(center, 4.0f, IM_COL32(255, 255, 255, 255));
    ImGui::End();
    ImGui::PopStyleVar(1);
#endif // _DEBUG
}
