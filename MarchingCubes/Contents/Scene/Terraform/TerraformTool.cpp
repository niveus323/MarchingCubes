#include "pch.h"
#include "TerraformTool.h"
#include "Core/Input/InputState.h"
#include "Core/Math/PhysicsHelper.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Geometry/MarchingCubes/Class/TerrainObject.h"
#include "Core/Geometry/Mesh/Component/StaticMeshComponent.h"
#include "Core/Geometry/MarchingCubes/TerrainFieldGenerator.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Assets/TextureRegistry.h"
#include "Core/Assets/TextureAsset.h"
#include "Core/Utils/FileUtils.h"
#include "Core/Engine/Subsystem/SceneSubsystem/TerrainSystem.h"
#include <DirectXTex.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>

BEGIN_REFLECTION(TerraformTool, IEditorTool)
END_REFLECTION()

void TerraformTool::OnActivated(EditorController* controller)
{
	IEditorTool::OnActivated(controller);

	if (m_scene->GetSubsystem<TerrainSystem>() == nullptr)
	{
		m_scene->AddSubsystem<TerrainSystem>();
	}
	
	// Debug Brush 생성
	m_debugBrush = m_scene->CreateObject<SceneObject>("BrushSphere", EObjectFlags::Transient | EObjectFlags::EditorOnly | EObjectFlags::Invisible);
	if (auto meshComp = m_debugBrush->AddComponent<StaticMeshComponent>())
	{
		meshComp->SetMeshByPath("@WireSphere");
	}
	m_debugBrush->SetActive(false);
}

void TerraformTool::OnDeactivated()
{
	IEditorTool::OnDeactivated();

	// 저장하지 않은 지형 오브젝트가 존재할 경우 저장을 유도해주는것이 좋아보임.
}

void TerraformTool::Update(float deltaTime)
{
	// 드래그 중 타이머 갱신
	if (m_bDraggingRegion || m_bResizingRegion)
	{
		m_updateTimer += deltaTime;
	}
}

bool TerraformTool::ProcessInput(const InputState* input, float deltaTime)
{
	if (!m_selectedTerrain) return false;

	if (input->IsPressed(ActionKey::LeftClick))
	{
		auto* camera = m_controller->GetScene()->GetMainCamera();
		MousePos mouse = input->GetMousePos();
		float mouseX = static_cast<float>(mouse.x);
		float mouseY = static_cast<float>(mouse.y);
		auto ray = PhysicsUtil::MakeRay(mouseX, mouseY, camera->GetViewportWidth(), camera->GetViewportHeight(), camera->GetViewProjMatrix());

		XMMATRIX worldMat = m_selectedTerrain->GetWorldMatrix();
		GridDesc setting = m_selectedTerrain->GetSetting();
		auto target = PhysicsUtil::RaymarchingTarget{
			.data = m_selectedTerrain->GetData().get(),
			.resolution = setting.resolution,
			.cellSize = setting.cellsize,
			.isoValue = setting.isoValue,
			.worldMatrix = worldMat,
			.userData = m_selectedTerrain
		};

		PhysicsUtil::HitResult result;
		if (PhysicsUtil::IsHit(target, ray, result))
		{
			const auto& hitPos = result.hitPos;
#ifdef _DEBUG
			// Hit가 발생한 위치에 원 세팅
			m_debugBrush->SetPosition(hitPos);
			m_debugBrush->SetActive(true);
#endif // DEBUG
			XMVECTOR vHitposLS = XMVector3TransformCoord(XMLoadFloat3(&hitPos), XMMatrixInverse(nullptr, m_selectedTerrain->GetWorldMatrix()));
			XMFLOAT3 hitposLS;
			XMStoreFloat3(&hitposLS, vHitposLS);

			float brushDelta = deltaTime * m_brushStrength * (EngineCore::GetInputState()->IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f);
			m_selectedTerrain->ApplyBrush(deltaTime * m_brushStrength, hitposLS, m_brushRadius);

			return true;
		}
	}

	return false;
}

void TerraformTool::RenderUI(IUIBuilder* ui)
{
	std::string fileName = "No Terrain Selected";
	bool bHasSelection = (m_selectedTerrain != nullptr);
	bool bIsDirty = false;

	if (bHasSelection)
	{
		std::filesystem::path statedPath = m_selectedTerrain->GetAssetPath();
		fileName = statedPath.filename().string();
		bIsDirty = m_selectedTerrain->IsDirty();

		if (bIsDirty) fileName += " *";
	}

	bool bIsOpen = true;
	if (ui->BeginPanel(fileName.c_str(), &bIsOpen))
	{
		if (ui->BeginTabBar("Tools"))
		{
			if (ui->BeginTabItem("Create"))
			{
				ManageTabUI(ui);
				ui->EndTabItem();
			}

			ui->BeginDisabled(!m_selectedTerrain);
			if (ui->BeginTabItem("Edit Chunk"))
			{
				if (ui->BeginTabBar("Edit Bar"))
				{
					if (ui->BeginTabItem("Edit"))
					{
						// TODO : 청크 선택

						// TODO : 청크 추가/삭제/이동

						ui->EndTabItem();
					}

					if (ui->BeginTabItem("Brush"))
					{
						BrushTabUI(ui);
						ui->EndTabItem();
					}

					if (ui->BeginTabItem("Noise"))
					{
						NoiseTabUI(ui);
						ui->EndTabItem();
					}
					ui->EndTabBar();
				}
				ui->EndTabItem();
			}
			ui->EndDisabled();

			if (ui->BeginTabItem("Visualization"))
			{
				VisualizationTabUI(ui);
				ui->EndTabItem();
			}

			ui->EndTabBar();
		}
	}
	ui->EndPanel();

	if (!bIsOpen && m_controller)
	{
		m_controller->SetTool(nullptr);
	}
}

void TerraformTool::SetBrushRadius(float radius)
{
	m_brushRadius = radius;
	if (m_debugBrush)
	{
		m_debugBrush->SetScale(XMFLOAT3{ radius, radius, radius });
	}
}

void TerraformTool::OnSelectionUpdated(GameObject* selected)
{
	//기존 선택했던 지형 오브젝트의 show 세팅 변경
	if (m_selectedTerrain)
	{
		m_selectedTerrain->ShowTerrainBound(false);
		m_selectedTerrain->ShowChunkBounds(false);
		m_selectedTerrain->ShowWireframe(false);
	}

	if (TerrainObject* selectedTerrain = dynamic_cast<TerrainObject*>(selected))
	{
		m_currentImagePath = "";
		m_isImageLoaded = false;
		
		m_selectedTerrain = selectedTerrain;
		const GridDesc& currentDesc = m_selectedTerrain->GetSetting();
		m_bDirtyRegion = false;
		m_updateTimer = 0.0f;

		m_selectedTerrain->ShowTerrainBound(m_bShowGrid);
		m_selectedTerrain->ShowChunkBounds(m_bShowGrid);
		m_selectedTerrain->ShowWireframe(m_bWireframe);
	}
	else
	{
		m_selectedTerrain = nullptr;
	}
}

void TerraformTool::ManageTabUI(IUIBuilder* ui)
{
	// --- Create New 패널 ---
	if (ui->BeginTabBar("ManageTabBar"))
	{
		ui->BeginDisabled(m_selectedTerrain);
		if (ui->BeginTabItem("Create New"))
		{
			//(1) 새 지형 생성
			ui->BeginTable("Create New Terrain", 2);

			if (ui->Property("World Scale", &m_targetScale))
			{
				if (m_targetScale.x <= 0.0f || m_targetScale.y <= 0.0f || m_targetScale.z <= 0.0f)
				{
					m_targetScale = XMFLOAT3(1.0f, 1.0f, 1.0f);
				}
			}
			ui->Property("Cells Per Chunk", &m_targetCellsPerChunk);
			ui->Property("Chunk Counts", &m_targetChunkCount);
			ui->Property("IsoValue", &m_targetIsoValue);
			//TODO : LOD 분할 기능 추가 시 주석 해제.
			//ui->Property("Submeshes Per Chunk", &m_targetSubmeshesPerChunk);
			ui->EndTable();

			ui->Separator();
			std::string info = "--- Estimated Terrain Info ---";
			ui->AlignNextItem(UI::UI_Alignment::AlignCenter, ui->CalcTextSize(info.c_str()).x);
			ui->Text(info);
			XMUINT3 totalResolution = XMUINT3(m_targetChunkCount.x * m_targetCellsPerChunk, m_targetChunkCount.y * m_targetCellsPerChunk, m_targetChunkCount.z * m_targetCellsPerChunk);
			ui->TextFormatted("Overall Resolution: %lu x %lu x %lu", totalResolution.x, totalResolution.y, totalResolution.z);
			ui->Separator();

			ui->BeginTable("Primitives", 2);
			// 도형 타입 선택 (ComboBox)
			static std::vector<std::string> primNames = { "Empty", "Plane", "Sphere" };
			static std::vector<int> primValues = { (int)EPrimitiveType::Empty, (int)EPrimitiveType::Plane, (int)EPrimitiveType::Sphere };

			int currentType = (int)m_primType;
			if (ui->PropertyEnum("Type", &currentType, primNames, primValues))
			{
				m_primType = (EPrimitiveType)currentType;
			}
			ui->Separator();

			// 타입별 파라미터 UI
			switch (m_primType)
			{
				case EPrimitiveType::Empty:
					ui->Text("Clears the entire terrain.");
					break;

				case EPrimitiveType::Plane:
					ui->Property("Height (Y)", &m_primHeight, 0.5f);
					break;

				case EPrimitiveType::Sphere:
					ui->Property("Radius", &m_primRadius, 0.5f);
					break;
			}
			ui->EndTable();
			ui->Dummy({ 0, 10 });

			// 생성 버튼
			if (ui->Button("Create"))
			{
				GridDesc newDesc{
				.resolution = XMUINT3{
					m_targetChunkCount.x * m_targetCellsPerChunk,
					m_targetChunkCount.y * m_targetCellsPerChunk,
					m_targetChunkCount.z * m_targetCellsPerChunk
				},
				.cellsize = 1.0f,
				.cellsPerChunk = m_targetCellsPerChunk,
				.isoValue = m_targetIsoValue
				};

				CreatePrimitive(newDesc);
			}
			ui->EndTabItem();
		}

		if (ui->BeginTabItem("Import From File"))
		{
			auto getRelativeDisplayPath = [](const std::string& fullPath) -> std::string {
				if (fullPath.empty()) return "None Selected";

				std::filesystem::path path(fullPath);
#ifdef CONTENTS_ROOT
				std::filesystem::path root(CONTENTS_ROOT);
				std::error_code ec;

				// root 기준으로 path의 상대 경로 계산
				std::filesystem::path relPath = std::filesystem::relative(path, root, ec);

				// 에러가 없으며, 계산된 상대 경로가 비어있지 않은 경우 반환
				if (!ec && !relPath.empty())
				{
					std::string relStr = relPath.string();

					// 상대 경로에 ".." 이 포함되어 있지 않다면 순수 하위 경로이므로 상대 경로 반환
					if (relStr.find("..") == std::string::npos)
					{
						return relStr;
					}
				}
#endif
				return path.string();
			};


			ui->Dummy({ 0, 5.0f });
			ui->Text("Source Template:");
			if (ui->Button("Browse...##Source"))
			{
				std::string path = FileUtils::FileDialogs::OpenFile("Terrain Template Asset", "*.sdf");
				if (!path.empty()) m_importSourcePath = path;
			}
			ui->SameLine();
			ui->Text(getRelativeDisplayPath(m_importSourcePath).c_str());
			ui->Dummy({ 0, 5.0f });
			
			ui->Text("Save Destination:");
			if (ui->Button("Browse...##Dest"))
			{
				std::string path = FileUtils::FileDialogs::SaveFile("Save New Terrain Asset", "*.sdf");
				if (!path.empty()) m_importDestPath = path;
			}
			ui->SameLine();
			ui->Text(getRelativeDisplayPath(m_importDestPath).c_str());
			ui->Dummy({ 0, 10.0f });

			ui->BeginDisabled(m_importSourcePath.empty() || m_importDestPath.empty());
			if (ui->Button("Create from Template", { 150.0f, 0.0f }))
			{
				LoadTerrain();
			}
			ui->EndDisabled();
			ui->EndTabItem();
		}
		ui->EndTabBar();

		ui->EndDisabled();
	}
}

void TerraformTool::BrushTabUI(IUIBuilder* ui)
{
	ui->BeginTable("Brush", 2);

	if (ui->Property("Brush Radius", &m_brushRadius, 0.2f))
	{
		SetBrushRadius(m_brushRadius);
	}

	if (ui->Property("Brush Strength", &m_brushStrength, 1.0f))
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
		
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = EngineCore::GetRenderSystem()->GetTextureRegistry()->GetGpuHandle(m_heightmapHandle);
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
			// 0.1초(60fps 기준 6프레임)마다 한 번씩만 업데이트 & LOD(저해상도) 적용
			if (m_updateTimer >= 0.1f && m_bDirtyRegion)
			{
				ApplyNoiseToTerrain(true);
				m_updateTimer = 0.0f;
				m_bDirtyRegion = false;
			}
		}
		else if (wasDragging)
		{
			// 고 해상도로 한번 더 갱신
			ApplyNoiseToTerrain(false);
			m_bDirtyRegion = false;
		}
	}

	if (ui->Button("Load Image File"))
	{
		LoadHeightmapImage();
	}

	ui->BeginTable("ImportSettings", 2);
	ui->Property("Height Scale", &m_heightScale, 1.0f);
	ui->EndTable();

	if (ui->Button("Apply to Terrain"))
	{
		ApplyNoiseToTerrain(false);
	}
}

void TerraformTool::VisualizationTabUI(IUIBuilder* ui)
{
	ui->Text("Debug Visuals");
	ui->Separator();

	bool bHasSelection = (m_selectedTerrain != nullptr);
	ui->BeginDisabled(!bHasSelection);

	if (ui->Checkbox("Show Terrain Grid", &m_bShowGrid) && bHasSelection)
	{
		m_selectedTerrain->ShowTerrainBound(m_bShowGrid);
		m_selectedTerrain->ShowChunkBounds(m_bShowGrid);
	}

	if (ui->Checkbox("Wireframe Mode", &m_bWireframe) && bHasSelection)
	{
		m_selectedTerrain->ShowWireframe(m_bWireframe);
	}
	ui->EndDisabled();
}

void TerraformTool::LoadHeightmapImage()
{
	static const std::string s_initialFilePath = "";

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

	m_currentImagePath = pathStr;
	const auto* image = m_cpuHeightmapImage->GetImage(0, 0, 0);
	m_imgWidth = static_cast<int>(image->width);
	m_imgHeight = static_cast<int>(image->height);

	// 편의상 파일 리로드로 미리보기 텍스쳐 생성.
	// 최적화 시 ScratchImage -> TextureAsset 생성 함수 구현하여 적용.
	if (m_heightmapAsset = EngineCore::GetResourceManager()->LoadTextureAsset(path))
	{
		m_heightmapHandle = EngineCore::GetRenderSystem()->GetTextureRegistry()->LoadTexture(m_heightmapAsset);
		m_isImageLoaded = true;
	}

}

bool TerraformTool::DrawAndControlRegion(IUIBuilder* ui, void* textureID, float w, float h)
{
	// 변경 전 값 저장
	UI::Vector<float,2> prevLT = m_regionLT;
	UI::Vector<float,2> prevRB = m_regionRB;

	UI::Vector<float,2> canvasPos = ui->GetCursorScreenPos();
	ui->Image(textureID, { w, h });

	// 이미지와 동일한 위치에서 투명 버튼 생성
	ui->SetCursorScreenPos(canvasPos);
	ui->InvisibleButton("##ROI_Canvas", { w, h });

	bool isActive = ui->IsItemActive();
	auto mousePos = ui->GetMousePos();

	auto UVToScreen = [&](const UI::Vector<float,2>& uv) {
		return UI::Vector<float, 2>(canvasPos.x + uv.x * w, canvasPos.y + uv.y * h);
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
		UI::Vector<float, 2> delta = { mousePos.x - m_dragStartPos.x, mousePos.y - m_dragStartPos.y };
		UI::Vector<float, 2> deltaUV = { delta.x / w, delta.y / h };

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

void TerraformTool::ApplyNoiseToTerrain(bool bUseLOD)
{
	// 이미지 -> 지형 데이터 변환은 이제 다음의 Flow로 진행한다.
	// 1. 지형 선택 (새 지형을 만들어서 적용하고 싶을 경우 새 지형 생성 기능을 이용해서 생성한 후 적용) -> EditorController + Hierarchy/Inspector에서 타겟 체크
	// 2. 노이즈 이미지 선택 (LoadHeightmapImage)
	// 3. 적용할 이미지의 영역 선택(ApplyNosieToTerrain)
	// 4. 이미지 높이 값 추출
	// 5. 높이 값 -> SDF에 적용

	// 이미지 기반 SDF 데이터 생성
	if (!m_selectedTerrain || !m_cpuHeightmapImage) return;

	auto data = m_selectedTerrain->GetData();
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
	if (bUseLOD)
	{
		// 프리뷰 지형 메쉬에 적용
		Log::Print("TerraformTool NoiseTab", "bLod = true");
		m_selectedTerrain->SetActive(false);
		m_selectedTerrain->ShowTerrainBound(false);
		m_selectedTerrain->ShowChunkBounds(false);
		m_selectedTerrain->ShowWireframe(false);
		if (!m_previewTerrain)
		{
			m_previewTerrain = m_scene->CreateObject<TerrainObject>("PreviewTerrain", EObjectFlags::Transient | EObjectFlags::EditorOnly | EObjectFlags::Invisible);
		}

		m_previewTerrain->SetPosition(m_selectedTerrain->GetPosition());
		m_previewTerrain->SetRotation(m_selectedTerrain->GetRotation());
		XMFLOAT3 originalScale = m_selectedTerrain->GetScale();
		m_previewTerrain->SetScale(originalScale);
		
		GridDesc previewDesc = m_selectedTerrain->GetSetting();
		// 전체 크기는 동일하지만 해상도는 절반이어야한다 -> cellSize 2배, cellsPerChunk 0.5배.
		previewDesc.resolution.x /= 2;
		previewDesc.resolution.y /= 2;
		previewDesc.resolution.z /= 2;
		previewDesc.cellsize *= 2.0f;
		previewDesc.cellsPerChunk = std::max(1u, previewDesc.cellsPerChunk / 2);

		if (auto previewField = TerrainFieldGenerator::CreateFromImage(previewDesc, roiData, roiWidth, roiHeight, m_heightScale))
		{
			m_previewTerrain->InjectTerrain(previewDesc, previewField);
			m_previewTerrain->SetActive(true);
			m_previewTerrain->ShowChunkBounds(m_bShowGrid);
			m_previewTerrain->ShowTerrainBound(m_bShowGrid);
			m_previewTerrain->ShowWireframe(m_bWireframe);
		}
	}
	else
	{
		Log::Print("TerraformTool NoiseTab", "bLod = false");
		if (m_previewTerrain) 
		{
			m_previewTerrain->SetActive(false);
			m_previewTerrain->ShowChunkBounds(false);
			m_previewTerrain->ShowTerrainBound(false);
			m_previewTerrain->ShowWireframe(false);
		}

		if (auto resultField = TerrainFieldGenerator::CreateFromImage(m_selectedTerrain->GetSetting(), roiData, roiWidth, roiHeight, m_heightScale))
		{
			m_selectedTerrain->InjectTerrain(m_selectedTerrain->GetSetting(), resultField);
			m_selectedTerrain->SetActive(true);
			m_selectedTerrain->ShowTerrainBound(m_bShowGrid);
			m_selectedTerrain->ShowChunkBounds(m_bShowGrid);
			m_selectedTerrain->ShowWireframe(m_bWireframe);
		}
	}
}

void TerraformTool::CreatePrimitive(const GridDesc& desc)
{
	std::string path = FileUtils::FileDialogs::SaveFile("Terrain Asset", "*.sdf");
	if (path.empty()) return; // 경로 지정을 취소하면 지형 생성 자체를 중단

	std::shared_ptr<SdfField> newField;
	switch (m_primType)
	{
		case EPrimitiveType::Plane:
			newField = TerrainFieldGenerator::CreatePlane(desc, m_primHeight);
			break;
		case EPrimitiveType::Sphere:
			newField = TerrainFieldGenerator::CreateSphere(desc, m_primRadius, { 50.0f, 50.0f, 50.0f });
			break;
		default:
			newField = TerrainFieldGenerator::CreateEmpty(desc);
			break;
	}

	if (auto* newObj = m_scene->CreateObject<TerrainObject>("NewTerrain"))
	{
		newObj->InjectTerrain(desc, newField);
		
		// 생성된 경로를 에셋 경로로 지정하고 디스크에 즉시 기록
		newObj->SetAssetPath(path);
		newObj->SaveDataAsset(path);

		// 새로 만든 객체를 선택 상태로 변경
		m_controller->SelectObject(newObj);
	}
}

void TerraformTool::SaveTerrain(std::string_view path)
{
	if (path.starts_with("Unsaved"))
	{
		SaveTerrainAs();
		return;
	}

	if (m_selectedTerrain->SaveDataAsset(path))
	{
		Log::Print("Saved terrain to %s", path.data());
	}
}

void TerraformTool::SaveTerrainAs()
{
	std::string path = FileUtils::FileDialogs::SaveFile("Terrain Asset", "*.sdf");
	if (path.empty())
	{
		return; // 사용자가 파일 탐색기를 열고 취소 버튼을 눌렀을 경우를 고려하여 로그를 작성하지 않았음
	}
	m_selectedTerrain->SetAssetPath(path);
	SaveTerrain(path);
}

void TerraformTool::LoadTerrain()
{
	if (m_importSourcePath.empty() || m_importDestPath.empty()) return;
	
	if (!FileUtils::DuplicateFile(m_importSourcePath, m_importDestPath, true)) return;

	if (auto newTerrain = m_scene->CreateObject<TerrainObject>("NewTerrain"))
	{
		if (newTerrain->LoadDataAsset(m_importDestPath))
		{
			m_controller->SelectObject(newTerrain);

			// 로드 성공 시 다음 작업을 위해 UI 캐싱 상태 초기화
			m_importSourcePath.clear();
			m_importDestPath.clear();
		}
		else
		{
			Log::Print("Failed to load terrain data from %s", m_importDestPath.c_str());
		}
		
	}
}
