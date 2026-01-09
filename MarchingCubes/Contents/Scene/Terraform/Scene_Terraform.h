#pragma once
#include "Core/Scene/Scene.h"
#include "Core/Geometry/MarchingCubes/TerrainSystem.h"
#include "Core/UI/UIRenderer.h"
#include <array>

// Forward Declaration
class TerraformTool;

class Scene_Terraform : public Scene
{
public:
	Scene_Terraform();
    ~Scene_Terraform() = default;

	void Init() override;
	void InitUI(IUIRenderer* ui) override;
    void BeginEditor() override;
	void Update(float deltaTime) override;
	void Render() override;

private:
    void RenderMarchingCubesUI();

    //Marching Cubes
    std::shared_ptr<SdfField<float>> MakeSphereGrid(unsigned int N, float cellSize, float radius, XMFLOAT3 center, GridDesc& OutGridDesc);
    

private:
    // Marching Cubes
    TerrainSystem* m_terrainSystem = nullptr;
    SceneObject* m_terrainRenderer = nullptr;
    
    // Light
    SceneObject* m_directionalLight = nullptr;

    // Settings
    DirectX::XMFLOAT3 m_gridOrigin = { 0,0,0 };
    int m_gridTiles = 100;
    int m_cellSize = 1;
    float m_mcIso = 0.0f;

    // UI
    UI::FrameCallbackToken cameraUIToken = 0;
    UI::FrameCallbackToken marchingCubesUIToken = 0;

    // Tool
    std::shared_ptr<TerraformTool> m_terraformTool;
};

