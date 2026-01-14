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

private:
    // Marching Cubes
    TerrainSystem* m_terrainSystem = nullptr;
    SceneObject* m_terrainRenderer = nullptr;
    
    // Light
    SceneObject* m_directionalLight = nullptr;

    // UI
    UI::FrameCallbackToken cameraUIToken = 0;
    
    // Tool
    std::shared_ptr<TerraformTool> m_terraformTool;
};

