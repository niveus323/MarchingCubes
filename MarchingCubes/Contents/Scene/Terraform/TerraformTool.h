#pragma once
#include "Core/Scene/Object/Controller/Tool/EditorTool.h"
#include "Core/UI/Builder/UIBuilder.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

class TerrainSystem;
class SceneObject;
class Mesh;
namespace DirectX
{
	class ScratchImage;
}

enum class EPrimitiveType
{
	Empty,
	Plane,
	Sphere
};

class TerraformTool : public IEditorTool
{
public:
	TerraformTool(TerrainSystem* terrainSystem, SceneObject* renderer);
	~TerraformTool() override;

	void OnActivated(EditorController* controller) override;
	void OnDeactivated() override;
	void Update(float deltaTime) override;
	void ProcessInput(const InputState* input, float deltaTime) override;
	void RenderUI(IUIBuilder* ui) override;

	void SetBrushRadius(float radius);
	void SetBrushStrength(float strength) { m_brushStrength = strength; }

private:
	void OptionPanel(IUIBuilder* ui);
	void GeometryTabUI(IUIBuilder* ui);
	void BrushTabUI(IUIBuilder* ui);
	void NoiseTabUI(IUIBuilder* ui);
	void VisualizationTabUI(IUIBuilder* ui);
	void LoadHeightmapImage();
	bool DrawAndControlRegion(IUIBuilder* ui, void* textureID, float w, float h);
	void ApplyToTerrain(bool bUseLOD);
	void UpdateChunkBoundsMesh();
	void CreatePrimitive();
private:
	TerrainSystem* m_terrainSystem = nullptr;
	SceneObject* m_terrainRenderer = nullptr;
	SceneObject* m_debugBrush = nullptr;
	SceneObject* m_debugCell = nullptr;
	SceneObject* m_debugChunkObject = nullptr;
	std::unique_ptr<Mesh> m_cellMesh;
	std::unique_ptr<Mesh> m_brushMesh;
	std::unique_ptr<Mesh> m_chunkBoundMesh;

	// --- Settings ---
	DirectX::XMFLOAT3 m_gridOrigin = { -50.0f, -50.0f, -50.0f };
	DirectX::XMUINT3 m_resolution = { 100u, 100u, 100u };
	float m_cellSize = 1;
	unsigned int m_chunkSize = 50;
	float m_mcIso = 0.0f;

	// --- Geometry Tab ---
	EPrimitiveType m_primType = EPrimitiveType::Plane;
	float m_primRadius = 25.0f;           // Sphere 반지름
	DirectX::XMFLOAT3 m_primSize = { 50.0f, 10.0f, 50.0f }; // Box Half-Extents
	float m_primHeight = 0.0f;            // Plane 높이 (Y)

	// --- Bursh Tab ---
	float m_brushRadius = 1.0f;
	float m_brushStrength = 5.0f;

	// --- Noise Tab ---
	uint32_t m_heightmapTextureHandle = UINT32_MAX;
	float m_heightScale = 1.0f;
	// SDF 생성에 사용할 CPU Raw 픽셀 데이터
	std::unique_ptr<DirectX::ScratchImage> m_cpuHeightmapImage;
	int m_imgWidth = 0;
	int m_imgHeight = 0;

	// 현재 선택된 이미지 파일 경로 (중복 로드 방지)
	std::string m_currentFilePath = "";

	bool m_isImageLoaded = false;

	UI::Vector<float,2> m_regionLT = { 0.0f, 0.0f }; // 사각형 좌 상단
	UI::Vector<float,2> m_regionRB = { 1.0f, 1.0f }; // 사각형 우 하단
	bool m_bDraggingRegion = false;
	bool m_bResizingRegion = false;
	float m_updateTimer = 0.0f;
	UI::Vector<float,2> m_dragStartPos = { 0.0f, 0.0f };
	UI::Vector<float,2> m_regionStartMin = { 0.0f, 0.0f }; 
	UI::Vector<float,2> m_regionStartMax = { 0.0f, 0.0f };

	GridDesc m_realDesc{}; // 프리뷰 시 교체되는 원래 값
	bool m_bDescRenewed = false;
	bool m_bDirtyRegion = false;

	// --- Visualization Tab ---
	bool m_bShowGrid = false;
	bool m_bWireframe = false;

	uint64_t m_lastRendererRevision = 0;
};

