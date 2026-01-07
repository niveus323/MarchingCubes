#pragma once
#include "Core/Scene/Object/Controller/Tool/EditorTool.h"

class TerrainSystem;
class SceneObject;
class Mesh;

class TerraformTool : public IEditorTool
{
public:
	TerraformTool(TerrainSystem* terrainSystem, SceneObject* renderer) :
		m_terrainSystem(terrainSystem),
		m_terrainRenderer(renderer)
	{}
	void OnActivated(EditorController* controller) override;
	void OnDeactivated() override;
	void ProcessInput(const InputState* input, float deltaTime) override;
	void RenderUI(IUIBuilder* ui) override;

	void SetBrushRadius(float radius);
	void SetBrushStrength(float strength) { m_brushStrength = strength; }
private:
	TerrainSystem* m_terrainSystem = nullptr;
	SceneObject* m_terrainRenderer = nullptr;
	SceneObject* m_debugBrush = nullptr;
	SceneObject* m_debugCell = nullptr;
	std::unique_ptr<Mesh> m_cellMesh;
	std::unique_ptr<Mesh> m_brushMesh;

	float m_brushRadius = 1.0f;
	float m_brushStrength = 5.0f;
};

