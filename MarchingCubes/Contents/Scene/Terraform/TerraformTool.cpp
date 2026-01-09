#include "pch.h"
#include "TerraformTool.h"
#include "Core/Input/InputState.h"
#include "Core/Math/PhysicsHelper.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Geometry/MarchingCubes/TerrainSystem.h"
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Geometry/MarchingCubes/TerrainFieldGenerator.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Utils/FileUtils.h"
#include "Core/Utils/Timer.h"
#include <DirectXTex.h>
#include <filesystem>
#include <algorithm>
#include <cctype>

TerraformTool::TerraformTool(TerrainSystem* terrainSystem, SceneObject* renderer) :
	m_terrainSystem(terrainSystem),
	m_terrainRenderer(renderer)
{
}

TerraformTool::~TerraformTool() = default;

void TerraformTool::OnActivated(EditorController* controller)
{
	IEditorTool::OnActivated(controller);

	// Debug Terrain Cell 생성
	GeometryData debugcellData;
	m_terrainSystem->MakeDebugCell(debugcellData, false);
	m_cellMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugcellData, "TerrainCell");
	m_debugCell = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugCell->AddComponent<MeshComponent>(m_cellMesh.get(), "Line");

	// Debug Brush 생성
	GeometryData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
	m_brushMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugBrushData, "DebugBrush");
	m_debugBrush = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugBrush->AddComponent<MeshComponent>(m_brushMesh.get(), "Wire");

	m_realDesc = m_terrainSystem->GetGridDesc();
}

void TerraformTool::OnDeactivated()
{

	IEditorTool::OnDeactivated();
}

void TerraformTool::Update(float deltaTime)
{
	// 드래그 중 타이머 갱신
	if (m_bDraggingRegion || m_bResizingRegion)
	{
		m_updateTimer += deltaTime;
	}
}

void TerraformTool::ProcessInput(const InputState* input, float deltaTime)
{
	if (input->IsPressed(ActionKey::LeftClick))
	{
		auto* camera = m_owner->GetScene()->GetMainCamera();

		MeshChunkRenderer* terrainRenderer = m_terrainSystem->GetRenderer();

		MousePos mouse = input->GetMousePos();
		float mouseX = static_cast<float>(mouse.x);
		float mouseY = static_cast<float>(mouse.y);

		auto ray = PhysicsUtil::MakeRay(mouseX, mouseY,
			camera->GetViewportWidth(),
			camera->GetViewportHeight(),
			camera->GetViewProjMatrix());

		std::vector<PhysicsUtil::RaycastTarget> targets;
		auto& terrainChunks = terrainRenderer->GetChunkSlots();
		for (const auto& chunk : terrainChunks)
		{
			targets.push_back(PhysicsUtil::RaycastTarget{
				.data = &chunk->meshData,
				.bounds = chunk->bounds,
				.worldMatrix = m_terrainRenderer->GetWorldMatrix()
				});
		}

		// RayCast로 terrainMesh를 피킹중인지 확인
		PhysicsUtil::RaycastHitResult result;
		if (PhysicsUtil::IsHit(targets, ray, result))
		{
			const auto& hitPos = result.hitPos;
#ifdef _DEBUG
			// Hit가 발생한 위치에 원 세팅
			m_debugBrush->SetPosition(hitPos);
#endif // DEBUG
			XMVECTOR vHitposLS = XMVector3TransformCoord(XMLoadFloat3(&hitPos), XMMatrixInverse(nullptr, m_terrainRenderer->GetWorldMatrix()));
			XMFLOAT3 hitposLS;
			XMStoreFloat3(&hitposLS, vHitposLS);

			BrushRequest req_brush{
				.deltaTime = deltaTime,
				.center = hitposLS,
				.radius = m_brushRadius,
				.weight = m_brushStrength * (EngineCore::GetInputState()->IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f)
			};
			m_terrainSystem->RequestBrush(req_brush);
		}
	}

}

void TerraformTool::RenderUI(IUIBuilder* ui)
{
	ui->BeginPanel("Terraform Tool");
	
	if (ui->BeginTabBar("Tools"))
	{
		if (ui->BeginTabItem("Brush", nullptr))
		{
			BrushTabUI(ui);
			ui->EndTabItem();
		}

		if (ui->BeginTabItem("Noise", nullptr))
		{
			NoiseTabUI(ui);
			ui->EndTabItem();
		}


		if (ui->BeginTabItem("Visualization", nullptr))
		{
			VisualizationTabUI(ui);
			ui->EndTabItem();
		}

		ui->EndTabBar();
	}
	
	ui->EndPanel();
}

void TerraformTool::SetBrushRadius(float radius)
{
	m_brushRadius = radius;
	if (m_debugBrush)
	{
		GeometryData newBrushMeshData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		m_brushMesh->UpdateData(EngineCore::GetUploadContext(), newBrushMeshData);
	}
}

void TerraformTool::BrushTabUI(IUIBuilder* ui)
{
	ui->BeginTable("Brush", 2);

	//ImGui::DragFloat("##Brush Radius", &m_brushRadius, 0.2f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (ui->PropertyFloat("Brush Radius", &m_brushRadius, 0.2f))
	{
		SetBrushRadius(m_brushRadius);
	}

	//ImGui::DragFloat("##Brush Strength", &m_brushStrength, 1.0f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (ui->PropertyFloat("Brush Strength", &m_brushStrength, 1.0f))
	{
		SetBrushStrength(m_brushStrength);
	}

	ui->EndTable();
}

void TerraformTool::NoiseTabUI(IUIBuilder* ui)
{
	ui->Text("Heightmap Image");

	// 로드된 이미지가 있다면 미리보기 출력
	if (m_isImageLoaded) 
	{
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = EngineCore::GetResourceManager()->GetTextureGpuHandle(m_heightmapTextureHandle);

		float aspect = (float)m_imgHeight / (float)m_imgWidth;
		float dispW = 300.0f;
		float dispH = dispW * aspect;
		bool wasDragging = m_bDraggingRegion || m_bResizingRegion;
		if (DrawAndControlRegion(ui, reinterpret_cast<void*>(gpuHandle.ptr), dispW, dispH))
		{
			m_bDirtyRegion = true;
		}

		if (m_bDraggingRegion || m_bResizingRegion)
		{
			// 0.05초(20 FPS)마다 한 번씩만 업데이트 & LOD(저해상도) 적용
			if (m_updateTimer >= 0.05f && m_bDirtyRegion)
			{
				ApplyToTerrain(true);
				m_updateTimer = 0.0f;
				m_bDirtyRegion = false;
			}
		}
		else if (wasDragging)
		{
			// 고 해상도로 한번 더 갱신
			ApplyToTerrain(false);
			m_bDirtyRegion = false;
		}
	}

	if (ui->Button("Load Image File")) 
	{
		LoadHeightmapImage();
	}

	ui->BeginTable("ImportSettings", 2);
	ui->PropertyFloat("Height Scale", &m_heightScale, 1.0f);
	ui->EndTable();

	if (ui->Button("Apply to Terrain"))
	{
		ApplyToTerrain(false);
	}
}

void TerraformTool::VisualizationTabUI(IUIBuilder* ui)
{
	ui->Text("Debug Visuals");
	ui->Separator();

	static bool bShowGrid = true;
	if (ui->Checkbox("Show Terrain Grid", &bShowGrid)) 
	{
		// m_debugCell->SetEnabled(bShowGrid);
	}

	static bool bWireframe = false;
	if (ui->Checkbox("Wireframe Mode", &bWireframe)) 
	{
		// 렌더링 시스템 모드 변경 로직
	}
}

void TerraformTool::LoadHeightmapImage()
{
	std::string pathStr = FileUtils::OpenFileDialog();
	if (pathStr.empty()) return;

	std::filesystem::path path(pathStr);
	m_cpuHeightmapImage = std::make_unique<DirectX::ScratchImage>();
	
	std::wstring ext = path.extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

	// 노이즈 이미지는 png, jpg, bmp 등 WIC 파일로만 사용
	ThrowIfFailed(DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, *m_cpuHeightmapImage));

	DirectX::ScratchImage converted;
	if (m_cpuHeightmapImage->GetMetadata().format != DXGI_FORMAT_R8_UNORM)
	{
		DirectX::Convert(*m_cpuHeightmapImage->GetImage(0, 0, 0), DXGI_FORMAT_R8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, converted);
		*m_cpuHeightmapImage = std::move(converted);
	}

	m_currentFilePath = pathStr;
	const auto* image = m_cpuHeightmapImage->GetImage(0, 0, 0);
	m_imgWidth = static_cast<int>(image->width);
	m_imgHeight = static_cast<int>(image->height);

	// 편의상 파일 리로드로 미리보기 텍스쳐 생성.
	// 최적화 시 ScratchImage -> TextureAsset 생성 함수 구현하여 적용.
	m_heightmapTextureHandle = EngineCore::GetResourceManager()->LoadTexture(path);

	m_isImageLoaded = true;
}

bool TerraformTool::DrawAndControlRegion(IUIBuilder* ui, void* textureID, float w, float h)
{
	// 변경 전 값 저장
	UI::Vector2 prevLT = m_regionLT;
	UI::Vector2 prevRB = m_regionRB;

	UI::Vector2 canvasPos = ui->GetCursorScreenPos();
	ui->Image(textureID, { w, h });

	// 이미지와 동일한 위치에서 투명 버튼 생성
	ui->SetCursorScreenPos(canvasPos);
	ui->InvisibleButton("##ROI_Canvas", { w, h });

	bool isActive = ui->IsItemActive();
	auto mousePos = ui->GetMousePos();

	auto UVToScreen = [&](const UI::Vector2& uv) {
		return UI::Vector2(canvasPos.x + uv.x * w, canvasPos.y + uv.y * h);
		};

	// 화면 좌표 계산
	auto screenMin = UVToScreen({ m_regionLT.x, m_regionLT.y });
	auto screenMax = UVToScreen({ m_regionRB.x, m_regionRB.y });

	// 핸들(우하단) 히트박스 체크
	float handleSize = 10.0f;
	bool hoverHandle = (mousePos.x >= screenMax.x - handleSize && mousePos.x <= screenMax.x + handleSize &&
		mousePos.y >= screenMax.y - handleSize && mousePos.y <= screenMax.y + handleSize);

	bool hoverBody = (mousePos.x >= screenMin.x && mousePos.x <= screenMax.x &&
		mousePos.y >= screenMin.y && mousePos.y <= screenMax.y);

	if (isActive && ui->IsMouseClicked(0))
	{
		m_dragStartPos = { mousePos.x, mousePos.y };
		m_regionStartMin = m_regionLT;
		m_regionStartMax = m_regionRB;

		if (hoverHandle) m_bResizingRegion = true;
		else if (hoverBody) m_bDraggingRegion = true;
	}

	if (ui->IsMouseReleased(0))
	{
		m_bDraggingRegion = false;
		m_bResizingRegion = false;
	}

	if (ui->IsMouseDragging(0))
	{
		UI::Vector2 delta = { mousePos.x - m_dragStartPos.x, mousePos.y - m_dragStartPos.y };
		UI::Vector2 deltaUV = { delta.x / w, delta.y / h };

		if (m_bResizingRegion)
		{
			m_regionRB.x = std::clamp(m_regionStartMax.x + deltaUV.x, m_regionLT.x + 0.05f, 1.0f);
			m_regionRB.y = std::clamp(m_regionStartMax.y + deltaUV.y, m_regionLT.y + 0.05f, 1.0f);
		}
		else if (m_bDraggingRegion)
		{
			float width = m_regionStartMax.x - m_regionStartMin.x;
			float height = m_regionStartMax.y - m_regionStartMin.y;

			m_regionLT.x = m_regionStartMin.x + deltaUV.x;
			m_regionLT.y = m_regionStartMin.y + deltaUV.y;
			m_regionRB.x = m_regionLT.x + width;
			m_regionRB.y = m_regionLT.y + height;

			// 경계 검사
			if (m_regionLT.x < 0.0f) { m_regionLT.x = 0.0f; m_regionRB.x = width; }
			if (m_regionLT.y < 0.0f) { m_regionLT.y = 0.0f; m_regionRB.y = height; }
			if (m_regionRB.x > 1.0f) { m_regionRB.x = 1.0f; m_regionLT.x = 1.0f - width; }
			if (m_regionRB.y > 1.0f) { m_regionRB.y = 1.0f; m_regionLT.y = 1.0f - height; }
		}
	}

	UI::Color dimColor = { 0.0f, 0.0f, 0.0f, 0.4f }; // 어두운 배경
	ui->DrawRectFilled(canvasPos, { canvasPos.x + w, screenMin.y }, dimColor); // 상단
	ui->DrawRectFilled({ canvasPos.x, screenMax.y }, { canvasPos.x + w, canvasPos.y + h }, dimColor); // 하단
	ui->DrawRectFilled({ canvasPos.x, screenMin.y }, { screenMin.x, screenMax.y }, dimColor); // 좌측
	ui->DrawRectFilled({ screenMax.x, screenMin.y }, { canvasPos.x + w, screenMax.y }, dimColor); // 우측
	ui->DrawRect(screenMin, screenMax, { 1.0f, 1.0f, 0.0f, 1.0f }); // 테두리 (노란색)

	// 리사이즈 핸들
	UI::Color handleColor = (hoverHandle || m_bResizingRegion) ? UI::Color{ 1.0f, 0.0f, 0.0f, 1.0f } : UI::Color{ 1.0f, 1.0f, 0.0f, 0.8f };
	ui->DrawRectFilled({ screenMax.x - handleSize, screenMax.y - handleSize }, screenMax, handleColor);

	// 실제 변경이 일어났는지 체크
	const float epsilon = 1e-5f;
	bool isChanged = (std::abs(prevLT.x - m_regionLT.x) > epsilon) ||
		(std::abs(prevLT.y - m_regionLT.y) > epsilon) ||
		(std::abs(prevRB.x - m_regionRB.x) > epsilon) ||
		(std::abs(prevRB.y - m_regionRB.y) > epsilon);

	return isChanged;
}

void TerraformTool::ApplyToTerrain(bool bUseLOD)
{
	// 이미지 기반 SDF 데이터 생성
	if (!m_terrainSystem || !m_cpuHeightmapImage) return;

	GridDesc currentDesc = m_terrainSystem->GetGridDesc();
	if (bUseLOD)
	{
		if (!m_bDescRenewed)
		{
			// 원본 GridDesc 최신화
			m_realDesc = m_terrainSystem->GetGridDesc();
			m_bDescRenewed = true;

			// 실시간 프리뷰 생성은 해상도를 절반으로 만들어(원본의 격자 크기는 유지하기 위해 셀 크기 2배) 연산량을 줄임
			currentDesc.cellsize *= 2.0f;
			currentDesc.cells.x /= 2;
			currentDesc.cells.y /= 2;
			currentDesc.cells.z /= 2;
		}
	}
	else
	{
		currentDesc = m_realDesc;
		m_bDescRenewed = false;
	}

	const DirectX::Image* resImg = m_cpuHeightmapImage->GetImage(0, 0, 0);

	// 이미지 좌표 -> 픽셀 좌표 변환
	int startX = static_cast<int>(m_regionLT.x * m_imgWidth);
	int startY = static_cast<int>(m_regionLT.y * m_imgHeight);
	int endX = static_cast<int>(m_regionRB.x * m_imgWidth);
	int endY = static_cast<int>(m_regionRB.y * m_imgHeight);

	// 경계 클램핑
	startX = std::clamp(startX, 0, m_imgWidth - 1);
	startY = std::clamp(startY, 0, m_imgHeight - 1);
	endX = std::clamp(endX, startX + 1, m_imgWidth);
	endY = std::clamp(endY, startY + 1, m_imgHeight);

	int roiWidth = endX - startX;
	int roiHeight = endY - startY;
	// Region 내 데이터 추출
	std::vector<uint8_t> roiData;
	roiData.reserve(roiWidth * roiHeight);
	for (int y = startY; y < endY; ++y)
	{
		const uint8_t* rowPtr = resImg->pixels + (y * resImg->rowPitch);
		roiData.insert(roiData.end(), rowPtr + startX, rowPtr + endX);
	}

	// 생성
	auto newField = TerrainFieldGenerator<float>::CreateFromImage(currentDesc, roiData, roiWidth, roiHeight, m_heightScale);
	if (newField != nullptr)
	{
		m_terrainSystem->SetGridDesc(currentDesc);
		m_terrainSystem->SetField(newField);
		m_terrainSystem->RequestRemesh();
	}
}
