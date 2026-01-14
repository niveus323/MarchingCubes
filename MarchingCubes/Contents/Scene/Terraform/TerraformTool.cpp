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
#include "Core/Rendering/RenderSystem.h"
#include "Core/Utils/FileUtils.h"
#include "Core/Utils/Timer.h"
#include "Core/Geometry/MarchingCubes/TerrainRendererComponent.h"
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
	if (!m_terrainSystem)
	{
		Log::Print("TerraformTool", "Invalid TerrainSubSystem!!!!");
		return;
	}

	if (!m_terrainSystem->IsLoaded())
	{
		GridDesc gridDesc{};
		gridDesc.resolution = m_resolution;
		gridDesc.cellsize = (float)m_cellSize;
		gridDesc.origin = m_gridOrigin;
		gridDesc.chunkSize = m_chunkSize;
		gridDesc.isoValue = m_mcIso;

		auto emptyField = TerrainFieldGenerator::CreateEmpty(gridDesc);
		m_terrainSystem->LoadTerrain(TerrainMode::CPU_MC33, gridDesc, emptyField, m_mcIso);
	}
	m_realDesc = m_terrainSystem->GetGridDesc();

	// Debug Terrain Cell 생성
	GeometryData debugcellData;
	m_terrainSystem->MakeDebugCell(debugcellData, false);
	m_cellMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugcellData, "TerrainCell");
	m_debugCell = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugCell->AddComponent<MeshComponent>(m_cellMesh.get(), "Line");
	m_debugCell->SetActive(false);

	// 청크 경계 메쉬 생성
	GeometryData emptyData;
	emptyData.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	m_chunkBoundMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), emptyData, "ChunkBounds");

	m_debugChunkObject = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugChunkObject->AddComponent<MeshComponent>(m_chunkBoundMesh.get(), "Line");
	m_debugChunkObject->SetActive(false);

	// Debug Brush 생성
	GeometryData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
	m_brushMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugBrushData, "DebugBrush");
	m_debugBrush = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugBrush->AddComponent<MeshComponent>(m_brushMesh.get(), "Wire");
	m_debugBrush->SetActive(false);
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

	if (m_terrainSystem)
	{
		MeshChunkRenderer* renderer = m_terrainSystem->GetRenderer();
		if (renderer)
		{
			uint64_t currentRev = renderer->GetRevision();

			// 렌더러의 버전이 내 버전보다 높으면 (=새로운 청크가 생성됨)
			if (currentRev != m_lastRendererRevision)
			{
				// 경계 메쉬 재생성
				UpdateChunkBoundsMesh();

				// 버전 동기화
				m_lastRendererRevision = currentRev;
			}
		}
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
			m_debugBrush->SetActive(true);
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

	OptionPanel(ui);

	if (ui->BeginTabBar("Tools"))
	{
		if (ui->BeginTabItem("Geometrty"))
		{
			GeometryTabUI(ui);
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

		if (ui->BeginTabItem("Visualization"))
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

void TerraformTool::OptionPanel(IUIBuilder* ui)
{
	if (ui->BeginTable("Marching Cubes Options", 2))
	{
		ui->Property("Origin", &m_gridOrigin);
		ui->Property("Num Of Tiles", &m_resolution);
		unsigned int resolutionMin = std::min(m_resolution.x, std::min(m_resolution.y, m_resolution.z));
		if (ui->Property("Cell Size", &m_cellSize))
		{
			m_cellSize = std::clamp(m_cellSize, 1.0f, static_cast<float>(resolutionMin));
		}
		if (ui->Property("ChunkSize", &m_chunkSize))
		{
			m_chunkSize = std::clamp<unsigned int>(m_chunkSize, 1u, resolutionMin);
		}

		ui->Separator();
		if (ui->Button("Apply Options"))
		{
			m_terrainSystem->SetGridDesc(GridDesc{
				.resolution = m_resolution,
				.cellsize = m_cellSize,
				.origin = m_gridOrigin,
				.chunkSize = m_chunkSize
			});
			m_terrainSystem->RequestRemesh();
		}
		ui->EndTable();
	}
}

void TerraformTool::GeometryTabUI(IUIBuilder* ui)
{
	ui->BeginTable("Create Primitives",2);

	// 1. 도형 타입 선택 (ComboBox)
	static std::vector<std::string> primNames = { "Empty", "Plane", "Sphere" };
	static std::vector<int> primValues = { (int)EPrimitiveType::Empty, (int)EPrimitiveType::Plane, (int)EPrimitiveType::Sphere };

	int currentType = (int)m_primType;
	if (ui->PropertyEnum("Type", &currentType, primNames, primValues))
	{
		m_primType = (EPrimitiveType)currentType;
	}

	ui->Separator();

	// 2. 타입별 파라미터 UI
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

	// 3. 생성 버튼
	if (ui->Button("Generate Geometry", { -1, 30 }))
	{
		CreatePrimitive();
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
	ui->Property("Height Scale", &m_heightScale, 1.0f);
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

	if (ui->Checkbox("Show Terrain Grid", &m_bShowGrid))
	{
		if (m_debugCell) m_debugCell->SetActive(m_bShowGrid);
		if (m_debugChunkObject) m_debugChunkObject->SetActive(m_bShowGrid);
	}

	static bool bWireframe = false;
	if (ui->Checkbox("Wireframe Mode", &bWireframe))
	{
		if (auto terrainComponent = m_terrainRenderer->GetComponent<TerrainRendererComponent>())
		{
			terrainComponent->SetPSO(bWireframe ? "Wire" : "Filled");
		}
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
			currentDesc.resolution.x /= 2;
			currentDesc.resolution.y /= 2;
			currentDesc.resolution.z /= 2;
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
	auto newField = TerrainFieldGenerator::CreateFromImage(currentDesc, roiData, roiWidth, roiHeight, m_heightScale);
	if (newField != nullptr)
	{
		m_terrainSystem->SetGridDesc(currentDesc);
		m_terrainSystem->SetField(newField);
		m_terrainSystem->RequestRemesh();
	}
}

void TerraformTool::UpdateChunkBoundsMesh()
{
	// 1. 초기화 및 유효성 검사
	if (!m_terrainSystem || !m_chunkBoundMesh) return;

	// 기능이 꺼져있으면 렌더링 끔
	if (!m_bShowGrid) return;

	MeshChunkRenderer* renderer = m_terrainSystem->GetRenderer();
	if (!renderer) return;

	// 2. 데이터 준비
	auto chunkSlots = renderer->GetChunkSlots();
	const GridDesc& desc = m_terrainSystem->GetGridDesc();

	GeometryData geom;
	geom.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

	// 청크의 물리적 크기 계산
	const float chunkWorldSize = desc.chunkSize * desc.cellsize;
	const float epsilon = 0.01f; // 부동소수점 오차 허용 범위
	const float viewBias = 0.05f; // 지형보다 살짝 위에 그리기 위한 오프셋

	// 3. 모든 청크 순회
	const auto& chunks = renderer->GetChunkMap();
	for (const auto& [key, slot] : chunks)
	{
		const auto& mesh = slot.meshData;
		if (mesh.indices.empty()) continue;

		float startX = desc.origin.x + key.x * chunkWorldSize;
		float startZ = desc.origin.z + key.z * chunkWorldSize;
		float endX = startX + chunkWorldSize;
		float endZ = startZ + chunkWorldSize;

		auto IsOnBoundaryX = [&](float x) {
			return (std::abs(x - startX) < epsilon) || (std::abs(x - endX) < epsilon);
			};
		auto IsOnBoundaryZ = [&](float z) {
			return (std::abs(z - startZ) < epsilon) || (std::abs(z - endZ) < epsilon);
			};

		// 4. 삼각형 엣지 순회 (index 3개씩)
		for (size_t i = 0; i < mesh.indices.size(); i += 3)
		{
			uint32_t i0 = mesh.indices[i];
			uint32_t i1 = mesh.indices[i + 1];
			uint32_t i2 = mesh.indices[i + 2];

			const Vertex& v0 = mesh.vertices[i0];
			const Vertex& v1 = mesh.vertices[i1];
			const Vertex& v2 = mesh.vertices[i2];

			// 엣지 검사 함수
			auto CheckAndAddEdge = [&](const Vertex& a, const Vertex& b)
				{
					// 두 정점이 모두 X 경계에 있거나, 모두 Z 경계에 있는 경우 -> 경계선 엣지!
					bool bOnX = IsOnBoundaryX(a.pos.x) && IsOnBoundaryX(b.pos.x);
					bool bOnZ = IsOnBoundaryZ(a.pos.z) && IsOnBoundaryZ(b.pos.z);

					if (bOnX || bOnZ)
					{
						Vertex va = a;
						Vertex vb = b;

						// [중요] Z-Fighting 방지: Normal 방향으로 살짝 띄움
						XMVECTOR nA = XMLoadFloat3(&a.normal);
						XMVECTOR pA = XMLoadFloat3(&a.pos);
						pA = XMVectorAdd(pA, XMVectorScale(nA, viewBias));
						XMStoreFloat3(&va.pos, pA);

						XMVECTOR nB = XMLoadFloat3(&b.normal);
						XMVECTOR pB = XMLoadFloat3(&b.pos);
						pB = XMVectorAdd(pB, XMVectorScale(nB, viewBias));
						XMStoreFloat3(&vb.pos, pB);

						// 색상은 튀는 색(밝은 초록 or 마젠타) 설정
						va.color = { 0.0f, 1.0f, 0.0f, 1.0f };
						vb.color = { 0.0f, 1.0f, 0.0f, 1.0f };

						uint32_t idx = (uint32_t)geom.vertices.size();
						geom.vertices.push_back(va);
						geom.vertices.push_back(vb);
						geom.indices.push_back(idx);
						geom.indices.push_back(idx + 1);
					}
				};

			// 삼각형의 3개 엣지 각각 검사
			CheckAndAddEdge(v0, v1);
			CheckAndAddEdge(v1, v2);
			CheckAndAddEdge(v2, v0);
		}
	}

	// 5. GPU 업로드
	if (!geom.vertices.empty())
	{
		m_chunkBoundMesh->UpdateData(EngineCore::GetUploadContext(), geom);
	}
	else
	{
		// 데이터가 없으면 클리어
		GeometryData empty;
		empty.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		m_chunkBoundMesh->UpdateData(EngineCore::GetUploadContext(), empty);
	}
}

void TerraformTool::CreatePrimitive()
{
	if (!m_terrainSystem) return;

	// 현재 Grid 설정 가져오기
	GridDesc desc = m_terrainSystem->GetGridDesc();

	// 만약 해상도 설정을 UI에서 바꿨다면 여기서 갱신 적용
	desc.resolution = m_resolution;
	desc.cellsize = m_cellSize;
	desc.origin = m_gridOrigin;
	desc.chunkSize = m_chunkSize;

	std::shared_ptr<SdfField> newField;
	switch (m_primType)
	{
		case EPrimitiveType::Plane:
			newField = TerrainFieldGenerator::CreatePlane(desc, m_primHeight);
			break;
		case EPrimitiveType::Sphere:
			newField = TerrainFieldGenerator::CreateSphere(desc, m_primRadius);
			break;
		default:
			newField = TerrainFieldGenerator::CreateEmpty(desc);
			break;
	}

	m_terrainSystem->SetGridDesc(desc);
	m_terrainSystem->SetField(newField);
	m_terrainSystem->RequestRemesh(); // 전체 리메쉬 수행
}
