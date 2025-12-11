#include "pch.h"
#include "FBXImporter.h"
#pragma warning(push)
#pragma warning(disable: 4251 4244 26451 26495 26813)
#include <fbxsdk.h>
#pragma warning(pop)

FBXImporter::FBXImporter()
{
	InitializeSDK();
}

FBXImporter::~FBXImporter()
{
	CleanupSDK();
}

void FBXImporter::InitializeSDK()
{
	m_manager = FbxManager::Create();
	if (!m_manager) return;

	FbxIOSettings* ios = FbxIOSettings::Create(m_manager, IOSROOT);
	m_manager->SetIOSettings(ios);
}

void FBXImporter::CleanupSDK()
{
	if (m_manager)
	{
		m_manager->Destroy();
		m_manager = nullptr;
	}
}

FBXImporter::ImportSceneData FBXImporter::LoadFile(const std::filesystem::path& path, const MeshImportOptions& options)
{
	ImportSceneData result;
	if (!m_manager) return result;

	fbxsdk::FbxImporter* importer = fbxsdk::FbxImporter::Create(m_manager, "");
	std::string pathStr = path.string();

	if (!importer->Initialize(pathStr.c_str(), -1, m_manager->GetIOSettings()))
	{
		Log::Print("FBXImporter", "Call to FbxImporter::Initialize() failed.\n");
		Log::Print("FBXImporter", "Error returned: %s\n", importer->GetStatus().GetErrorString());
		importer->Destroy();
		return result;
	}

	FbxScene* scene = FbxScene::Create(m_manager, "myScene");
	importer->Import(scene);
	importer->Destroy();

	// 좌표계 변환
	FbxAxisSystem directXSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
	directXSystem.ConvertScene(scene);

	FbxGeometryConverter converter(m_manager);
	converter.Triangulate(scene, true);

	ProcessScene(scene, result, options);

	scene->Destroy();
	result.success = !result.geometry.vertices.empty();
	return result;
}

void FBXImporter::ProcessScene(fbxsdk::FbxScene* scene, ImportSceneData& outData, const MeshImportOptions& options)
{
	std::unordered_map<FbxSurfaceMaterial*, uint32_t> matPtrToIndex;
	std::vector<FbxNode*> nodes;
	nodes.push_back(scene->GetRootNode());

	// Scene에서 사용된 Material 체크
	size_t head = 0;
	while (head < nodes.size())
	{
		FbxNode* curr = nodes[head++];
		const int matCount = curr->GetMaterialCount();
		for (int i = 0; i < matCount; ++i)
		{
			FbxSurfaceMaterial* mat = curr->GetMaterial(i);
			if (mat && !matPtrToIndex.contains(mat))
			{
				matPtrToIndex[mat] = (uint32_t)outData.materials.size();
				outData.materials.push_back(ParseMaterial(mat));
			}
		}
		for (int i = 0; i < curr->GetChildCount(); ++i) nodes.push_back(curr->GetChild(i));
	}

	// FbxMesh 수집
	std::vector<FbxMesh*> fbxMeshes;
	CollectMeshes(scene->GetRootNode(), fbxMeshes);
	outData.geometry.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// GeometryData에 Mesh 정보 저장
	for (auto* mesh : fbxMeshes)
	{
		PrintLayerInfo(mesh);
		AppendFbxMeshToGeometry(mesh, matPtrToIndex, outData.geometry, &outData.submeshes);
	}

	if (options.applyUnitConversion && options.uniformScale != 1.0f)
	{
		ApplyUniformScale(outData.geometry, options.uniformScale);
	}
}

void FBXImporter::CollectMeshes(fbxsdk::FbxNode* node, std::vector<fbxsdk::FbxMesh*>& outMeshes)
{
	if (!node) return;
	if (auto* mesh = node->GetMesh()) outMeshes.push_back(mesh);
	for (int i = 0; i < node->GetChildCount(); ++i) CollectMeshes(node->GetChild(i), outMeshes);
}

ImportedMaterialDesc FBXImporter::ParseMaterial(const fbxsdk::FbxSurfaceMaterial* fbxMat)
{
	ImportedMaterialDesc outMatDesc{};
	if (!fbxMat) return outMatDesc;

	outMatDesc.name = fbxMat->GetName();

	// 텍스처 경로 추출 람다
	auto extractTexturePath = [](const fbxsdk::FbxSurfaceMaterial* fbxMat, const char* propName) -> std::filesystem::path {
		FbxProperty prop = fbxMat->FindProperty(propName);
		if (!prop.IsValid()) return "";

		const int texCount = prop.GetSrcObjectCount<FbxTexture>();
		for (int i = 0; i < texCount; ++i)
		{
			FbxTexture* tex = prop.GetSrcObject<FbxTexture>(i);
			FbxFileTexture* fileTex = FbxCast<FbxFileTexture>(tex);
			if (!fileTex) continue;

			// FBX에 저장된 원본 경로 (절대 경로일 수 있음)
			const char* fileName = fileTex->GetFileName();
			if (!fileName) continue;

			return std::filesystem::path(reinterpret_cast<const char8_t*>(fileName));
		}
		return "";
	};

	// 색상 추출 람다
	auto extractColor = [](const fbxsdk::FbxSurfaceMaterial* fbxMat, const char* propName) {
		FbxProperty prop = fbxMat->FindProperty(propName);
		if (!prop.IsValid()) return XMFLOAT3(1.0f, 1.0f, 1.0f);
		FbxDouble3 c = prop.Get<FbxDouble3>();
		return XMFLOAT3((float)c[0], (float)c[1], (float)c[2]);
	};

	// 속성 추출 람다
	auto extractFactor = [](const fbxsdk::FbxSurfaceMaterial* fbxMat, const char* propName, float defVal) {
		FbxProperty prop = fbxMat->FindProperty(propName);
		if (!prop.IsValid()) return defVal;
		return (float)prop.Get<FbxDouble>();
	};

	outMatDesc.diffuseColor = extractColor(fbxMat, "DiffuseColor");

	// PBR Material
	if (fbxMat->GetClassId().Is(FbxSurfacePhong::ClassId) || fbxMat->GetClassId().Is(FbxSurfaceLambert::ClassId))
	{
		// Phong/Lambert 계열
		outMatDesc.diffusePath = extractTexturePath(fbxMat, FbxSurfaceMaterial::sDiffuse);
		outMatDesc.normalPath = extractTexturePath(fbxMat, FbxSurfaceMaterial::sNormalMap);
		outMatDesc.emissivePath = extractTexturePath(fbxMat, FbxSurfaceMaterial::sEmissive);
		// SpecularMap 등을 Roughness/Metallic으로 근사하거나 무시
	}
	else
	{
		outMatDesc.diffusePath = extractTexturePath(fbxMat, "DiffuseColor");
		if (outMatDesc.diffusePath.empty()) outMatDesc.diffusePath = extractTexturePath(fbxMat, "BaseColor");

		outMatDesc.normalPath = extractTexturePath(fbxMat, "NormalMap");
		outMatDesc.roughnessPath = extractTexturePath(fbxMat, "Roughness");
		outMatDesc.metallicPath = extractTexturePath(fbxMat, "Metallic");
		outMatDesc.emissivePath = extractTexturePath(fbxMat, "Emissive");

		outMatDesc.roughness = extractFactor(fbxMat, "Roughness", 0.5f);
		outMatDesc.metallic = extractFactor(fbxMat, "Metallic", 0.0f);
	}

	return outMatDesc;
}

void FBXImporter::AppendFbxMeshToGeometry(FbxMesh* fbxMesh, const std::unordered_map<FbxSurfaceMaterial*, uint32_t>& matPtrToGlobalIndex, GeometryData& outGeometry, std::vector<MeshSubmesh>* outSubmeshes)
{
	if (!fbxMesh) return;

	fbxMesh->RemoveBadPolygons();
	fbxMesh->GenerateNormals(true, true);

	const int polygonCount = fbxMesh->GetPolygonCount();
	FbxVector4* controlPoints = fbxMesh->GetControlPoints();

	FbxStringList uvSetNames;
	fbxMesh->GetUVSetNames(uvSetNames);
	const char* uvSetName = uvSetNames.GetCount() > 0 ? uvSetNames[0] : nullptr;

	FbxNode* node = fbxMesh->GetNode();

	// 현재 진행 중인 서브메시 정보
	MeshSubmesh currentSubmesh{}; 
	currentSubmesh.indexOffset = static_cast<uint32_t>(outGeometry.indices.size());

	int prevLocalMatIdx = -1;
	uint32_t currentGlobalMatIdx = UINT32_MAX;

	auto flushSubmesh = [&]() {
		if (outSubmeshes && currentSubmesh.indexCount > 0)
		{
			currentSubmesh.materialIndex = currentGlobalMatIdx;
			outSubmeshes->push_back(currentSubmesh);
		}
	};

	FbxGeometryElementUV* vertexUV = fbxMesh->GetElementUV(0);
	for (int polyIdx = 0; polyIdx < polygonCount; ++polyIdx)
	{
		const int polySize = fbxMesh->GetPolygonSize(polyIdx);
		if (polySize != 3) continue;

		const int localMatIdx = GetLocalMaterialIndex(fbxMesh, polyIdx);

		// Material이 변경되었거나, 루프의 첫 진입 시 처리
		if (polyIdx == 0 || localMatIdx != prevLocalMatIdx)
		{
			if (polyIdx > 0) flushSubmesh(); // 이전 서브메시 저장

			// 새 서브메시 초기화
			currentSubmesh.indexOffset = static_cast<uint32_t>(outGeometry.indices.size());
			currentSubmesh.indexCount = 0;

			// 로컬 인덱스 -> 전역 인덱스 변환
			FbxSurfaceMaterial* matPtr = node->GetMaterial(localMatIdx);
			if (matPtr && matPtrToGlobalIndex.contains(matPtr)) {
				currentGlobalMatIdx = matPtrToGlobalIndex.at(matPtr);
			}
			else {
				currentGlobalMatIdx = UINT32_MAX; // Material 없음
			}

			prevLocalMatIdx = localMatIdx;
		}

		// Vertex 생성
		for (int vertIdx = 0; vertIdx < polySize; ++vertIdx)
		{
			const int ctrlPointIndex = fbxMesh->GetPolygonVertex(polyIdx, vertIdx);
			FbxVector4 pos = controlPoints[ctrlPointIndex];

			Vertex v{};
			v.pos = XMFLOAT3(static_cast<float>(pos[0]), static_cast<float>(pos[1]), static_cast<float>(pos[2]));

			// normal
			FbxVector4 n;
			if (fbxMesh->GetPolygonVertexNormal(polyIdx, vertIdx, n))
			{
				n.Normalize();
				v.normal = XMFLOAT3(static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2]));
			}

			// uv
			if (vertexUV)
			{
				FbxVector2 uvValue(0, 0);
				int uvIndex = -1;

				// 매핑 모드에 따라 인덱스 추출 방식 결정
				switch (vertexUV->GetMappingMode())
				{
					case FbxGeometryElement::eByControlPoint:
						// 제어점 기준 (정점이 공유되면 UV도 공유)
						if (vertexUV->GetReferenceMode() == FbxGeometryElement::eDirect)
							uvIndex = ctrlPointIndex;
						else if (vertexUV->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
							uvIndex = vertexUV->GetIndexArray().GetAt(ctrlPointIndex);
						break;

					case FbxGeometryElement::eByPolygonVertex:
						// 폴리곤 버텍스 기준 (UV Seam이 있는 경우, 게임에서 가장 일반적)
						// FBX SDK가 제공하는 텍스처 인덱스 헬퍼 사용
						uvIndex = fbxMesh->GetTextureUVIndex(polyIdx, vertIdx);
						break;
				}

				// 인덱스가 유효하면 실제 데이터 배열에서 값 조회
				if (uvIndex != -1)
				{
					uvValue = vertexUV->GetDirectArray().GetAt(uvIndex);

					// DirectX 포맷에 맞춰 V(Y) 반전
					v.texCoord = XMFLOAT2(
						static_cast<float>(uvValue[0]),
						1.0f - static_cast<float>(uvValue[1])
					);
				}
			}

			outGeometry.vertices.push_back(v);
			outGeometry.indices.push_back(static_cast<uint32_t>(outGeometry.vertices.size() - 1));
		}

		if (outSubmeshes) currentSubmesh.indexCount += 3;
	}

	// 마지막 서브메시 확정
	flushSubmesh();
}

void FBXImporter::ApplyUniformScale(GeometryData& data, float s)
{
	for (auto& v : data.vertices)
	{
		v.pos.x *= s;
		v.pos.y *= s;
		v.pos.z *= s;
	}
}

int FBXImporter::GetLocalMaterialIndex(FbxMesh* mesh, int polyIndex)
{
	if (!mesh) return -1;

	FbxLayerElementMaterial* matElem = mesh->GetElementMaterial();
	if (!matElem) return -1;

	auto& indexArray = matElem->GetIndexArray();
	if (indexArray.GetCount() == 0) return -1;

	switch (matElem->GetMappingMode())
	{
		case FbxLayerElement::eByPolygon:
		{
			if (polyIndex < indexArray.GetCount())
				return indexArray[polyIndex];
		}
		break;
		case FbxLayerElement::eAllSame:
			return indexArray[0];
		default:
			return indexArray[0];
	}

	return -1;
}

void FBXImporter::PrintLayerInfo(FbxMesh* pMeshNode)
{
#ifdef _DEBUG
	assert(pMeshNode);
	int layerCount = pMeshNode->GetLayerCount();
	Log::Print("FbxImporter", "Layer Count = %d", layerCount);
	for (int i = 0; i < layerCount; ++i)
	{
		FbxLayer* layer = pMeshNode->GetLayer(i);
		if (const FbxLayerElementTemplate<int>* polygons = layer->GetPolygonGroups())			Log::Print("FbxImporter", "Layer %d has Polygons", i);
		if (const FbxLayerElementTemplate<FbxVector4>* normal = layer->GetNormals())			Log::Print("FbxImporter", "Layer %d has normals", i);
		if (const FbxLayerElementTemplate<FbxVector4>* tangent = layer->GetTangents())			Log::Print("FbxImporter", "Layer %d has tangents", i);
		if (const FbxLayerElementTemplate<FbxVector4>* binormal = layer->GetBinormals())		Log::Print("FbxImporter", "Layer %d has binormals", i);
		if (const FbxLayerElementTemplate<FbxColor>* vertexColors = layer->GetVertexColors())	Log::Print("FbxImporter", "Layer %d has vertexColors", i);
		const auto uvset = layer->GetUVSets();
		if (uvset.Size() > 0) Log::Print("FbxImporter", "Layer %d has UVSets (%d sets)", i, uvset.Size());

		if (FbxLayerElementMaterial* materialElement = layer->GetMaterials())
		{
			Log::Print("FbxImporter", "Layer %d has materials", i);

			FbxNode* ownerNode = pMeshNode->GetNode();
			int materialCount = ownerNode ? ownerNode->GetMaterialCount() : 0;
			Log::Print("FbxImporter", "  Material count on node: %d", materialCount);

			for (int j = 0; j < materialCount; ++j)
			{
				FbxSurfaceMaterial* mat = ownerNode->GetMaterial(j);
				if (!mat)	continue;

				Log::Print("FbxImporter", "  [Material %d]", j);
				Log::Print("FbxImporter", "    Name         : %s", mat->GetName());
				Log::Print("FbxImporter", "    ShadingModel : %s", mat->ShadingModel.Get().Buffer());

				auto LogColorProp = [&](const char* label, const char* propName) {
					FbxProperty prop = mat->FindProperty(propName);
					if (!prop.IsValid())return;

					FbxDouble3 c = prop.Get<FbxDouble3>();
					Log::Print("FbxImporter", "    %-16s: (%f, %f, %f)", label, c[0], c[1], c[2]);
					};

				LogColorProp("Emissive", "Emissive");
				LogColorProp("Ambient", "Ambient");
				LogColorProp("Diffuse", "Diffuse");
				LogColorProp("Specular", "Specular");

				for (FbxProperty prop = mat->GetFirstProperty(); prop.IsValid(); prop = mat->GetNextProperty(prop))
				{
					const char* propName = prop.GetNameAsCStr();
					int srcCount = prop.GetSrcObjectCount();
					if (srcCount <= 0) continue;
					Log::Print("FbxImporter", "    Prop '%s' has %d src objects", propName, srcCount);

					for (int t = 0; t < srcCount; ++t)
					{
						FbxObject* src = prop.GetSrcObject(t);
						if (!src) continue;

						const char* className = src->GetClassId().GetName();
						const char* objName = src->GetName();
						Log::Print("FbxImporter", "        [%d] class=%s name=%s", t, className ? className : "(null)", objName ? objName : "(null)");
					}
				}
			}
		}
	}
#endif // _DEBUG
}
