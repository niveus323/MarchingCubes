#pragma once
#include "Core/Scene/Object/Controller/Tool/EditorTool.h"
#include "Core/UI/Builder/UIBuilder.h"
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"

enum class EPrimitiveType
{
	Empty,
	Plane,
	Sphere
};

//Forward Declaration
class SceneObject;
class TerrainObject;
class TerrainSystem;
class Mesh;
class TextureAsset;

namespace DirectX
{
	class ScratchImage;
}

class TerraformTool : public IEditorTool
{
	REFLECT_GENERATED_BODY(TerraformTool)
public:
	void OnActivated(EditorController* controller) override;
	void OnDeactivated() override;
	void Update(float deltaTime) override;
	bool ProcessInput(const InputState* input, float deltaTime) override;
	void RenderUI(IUIBuilder* ui) override;

	void SetBrushRadius(float radius);
	void SetBrushStrength(float strength) { m_brushStrength = strength; }
	void OnSelectionUpdated(GameObject* selected);

private:
	void ManageTabUI(IUIBuilder* ui);
	void BrushTabUI(IUIBuilder* ui);
	void NoiseTabUI(IUIBuilder* ui);
	void VisualizationTabUI(IUIBuilder* ui);
	void LoadHeightmapImage();
	bool DrawAndControlRegion(IUIBuilder* ui, void* textureID, float w, float h);
	void ApplyNoiseToTerrain(bool bUseLOD);
	void CreatePrimitive(const GridDesc& desc); // 설정한 설정 값으로 새 Terrain 생성
	void SaveTerrain(std::string_view path);
	void SaveTerrainAs();
	void LoadTerrain();

private:
	SceneObject* m_debugBrush = nullptr; // BrushCursor

	// --- Geometry Tab ---
	EPrimitiveType m_primType = EPrimitiveType::Plane;
	float m_primRadius = 25.0f;									// Sphere 반지름
	DirectX::XMFLOAT3 m_primSize = { 50.0f, 10.0f, 50.0f };		// Box Half-Extents
	float m_primHeight = 0.0f;									// Plane 높이 (Y)

	// --- Bursh Tab ---
	float m_brushRadius = 1.0f;
	float m_brushStrength = 5.0f;

	// --- Noise Tab ---
	std::shared_ptr<TextureAsset> m_heightmapAsset;
	uint32_t m_heightmapHandle = UINT32_MAX;
	float m_heightScale = 1.0f;
	// SDF 생성에 사용할 CPU Raw 픽셀 데이터
	std::unique_ptr<DirectX::ScratchImage> m_cpuHeightmapImage;
	int m_imgWidth = 0;
	int m_imgHeight = 0;

	// 현재 선택된 이미지 파일 경로 (중복 로드 방지)
	std::string m_currentImagePath = "";
	bool m_isImageLoaded = false;

	UI::Vector<float,2> m_regionLT = { 0.0f, 0.0f }; // 사각형 좌 상단
	UI::Vector<float,2> m_regionRB = { 1.0f, 1.0f }; // 사각형 우 하단
	bool m_bDraggingRegion = false;
	bool m_bResizingRegion = false;
	float m_updateTimer = 0.0f;
	UI::Vector<float,2> m_dragStartPos = { 0.0f, 0.0f };
	UI::Vector<float,2> m_regionStartMin = { 0.0f, 0.0f }; 
	UI::Vector<float,2> m_regionStartMax = { 0.0f, 0.0f };

	// --- Manage Panel ---
	DirectX::XMFLOAT3 m_targetScale{ 1.0f, 1.0f, 1.0f };
	uint32_t m_targetCellsPerChunk = 16u;
	DirectX::XMUINT3 m_targetChunkCount{ 1u,1u,1u };
	float m_targetIsoValue = 0.0f;
	uint32_t m_targetSubmeshesPerChunk = 1u;
	std::string m_importSourcePath = "";
	std::string m_importDestPath = "";

	// -- Noise Panel ---
	bool m_bDirtyRegion = false;
	TerrainObject* m_previewTerrain = nullptr;

	// --- Visualization Panel ---
	bool m_bShowGrid = false;
	bool m_bShowChunkBounds = false;
	bool m_bWireframe = false;

	// Target
	TerrainObject* m_selectedTerrain = nullptr;
};

