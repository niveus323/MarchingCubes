#pragma once
#include "Interface/EditorApp.h"
#include "Core/Geometry/MarchingCubes/TerrainSystem.h"
#include "Core/Scene/SceneObject.h"
#include "Core/Scene/BaseScene.h"
#include <unordered_map>
#include <array>

class MCTerraformEditor : public EditorApp
{
public:
    MCTerraformEditor(uint32_t width, uint32_t height, std::wstring name) : EditorApp(width, height, name) {}
    
	virtual void OnDestroy() override;
protected:
    virtual void InitScene(ID3D12GraphicsCommandList* cmd) override;
    virtual void InitUI() override;
    virtual void UpdateScene(float deltaTime) override;
    virtual void OnUpload(ID3D12GraphicsCommandList* cmd) override;
    virtual void DrawScene(ID3D12GraphicsCommandList* cmd) override;

private:
    void RenderCameraLightUI();
    void RenderMarchingCubesUI();

    //Marching Cubes
    std::shared_ptr<SdfField<float>> MakeSphereGrid(unsigned int N, float cell, float radius, XMFLOAT3 origin, GridDesc& OutGridDesc);

private:
    // Marching Cubes
    std::unique_ptr<TerrainSystem> m_terrain;
    SceneObject* m_debugBrush = nullptr;
    SceneObject* m_debugCell = nullptr;
    SceneObject* m_terrainRenderer = nullptr;
    SceneObject* m_whale = nullptr;
    
    // Settings
    DirectX::XMFLOAT3 m_gridOrigin = { 0,0,0 };
    int m_gridTiles = 100;
    int m_cellSize = 1;
    float m_brushRadius = 3.0f;
    float m_brushStrength = 5.0f;
    std::array<float, 3> m_lightDir = { -1.0f, -1.0f, -1.0f };
    float m_mcIso = 0.0f;
    
    // UI
    UI::FrameCallbackToken cameraUIToken = 0;
    UI::FrameCallbackToken marchingCubesUIToken = 0;
    
    // Scene
    std::unique_ptr<BaseScene> m_scene;
    std::vector<std::unique_ptr<Mesh>> m_editorMeshes;
};