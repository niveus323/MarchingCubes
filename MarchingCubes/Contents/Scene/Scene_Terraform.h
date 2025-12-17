#pragma once
#include "Core/Scene/Scene.h"
#include "Core/Geometry/MarchingCubes/TerrainSystem.h"
#include "Core/UI/UIRenderer.h"
#include <array>

// Forward Declaration
class Mesh;

class Scene_Terraform : public Scene
{
public:
	Scene_Terraform();
    ~Scene_Terraform() = default;

	void Init() override;
	void InitUI(IUIRenderer* ui) override;
	void Update(float deltaTime) override;
	void Render() override;

private:
    void RenderCameraLightUI();
    void RenderMarchingCubesUI();

    //Marching Cubes
    std::shared_ptr<SdfField<float>> MakeSphereGrid(unsigned int N, float cellSize, float radius, XMFLOAT3 center, GridDesc& OutGridDesc);

private:
    // Marching Cubes
    std::unique_ptr<TerrainSystem> m_terrain;
    SceneObject* m_debugBrush = nullptr;
    SceneObject* m_debugCell = nullptr;
    SceneObject* m_terrainRenderer = nullptr;
    SceneObject* m_whale = nullptr;

    // Light
    SceneObject* m_directionalLight = nullptr;

    // Settings
    DirectX::XMFLOAT3 m_gridOrigin = { 0,0,0 };
    int m_gridTiles = 100;
    int m_cellSize = 1;
    float m_brushRadius = 3.0f;
    float m_brushStrength = 5.0f;
    float m_mcIso = 0.0f;
    std::array<float, 3> m_lightDir = { -1.0f, -1.0f, -1.0f };
    float m_cameraSpeed = 100.0f;

    // UI
    UI::FrameCallbackToken cameraUIToken = 0;
    UI::FrameCallbackToken marchingCubesUIToken = 0;

    // Scene
    std::vector<std::unique_ptr<Mesh>> m_editorMeshes;
};

