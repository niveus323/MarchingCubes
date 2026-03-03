#include "pch.h"
#include "SceneObject.h"
#include "Core/Scene/Component/BillboardComponent.h"
#include "Core/Assets/ResourceManager.h"

BEGIN_REFLECTION(SceneObject, GameObject)
END_REFLECTION()

void SceneObject::Init()
{
    GameObject::Init();
    m_transformComp = AddComponent<TransformComponent>();

    auto billboard = AddComponent<BillboardComponent>(EObjectFlags::Transient | EObjectFlags::EditorOnly);
    // 아이콘 세팅
    auto iconMat = EngineCore::GetResourceManager()->LoadMaterialAsset(GetFullPath(AssetType::Default, L"Material/EditorBillboard.json"));
    billboard->SetMaterial(iconMat);
}
