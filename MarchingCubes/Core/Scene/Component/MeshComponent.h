#pragma once
#include "RendererComponent.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/Geometry/Mesh/Class/Mesh.h"
#include "Core/Assets/Material/MaterialAsset.h"
#include <DirectXCollision.h>

//Forward Declaration
class SceneObject;

class MeshComponent : public RendererComponent
{
	REFLECT_GENERATED_BODY(MeshComponent)
public:
	virtual void Init() override;
	virtual void Submit() override;
	virtual void Serialize(Serializer& ar) override;

	// Mesh Accessor
	virtual Mesh* GetMesh() const = 0;

	// Material Accessor
	size_t GetMaterialSlotCount() const { return m_materialInstnaces.size(); }
	void SetMaterialByPath(int slot, const std::string& matPath);
	void SetMaterial(int slot, std::shared_ptr<MaterialAsset> matAsset);
	void SetPSO(int slot, std::string_view psoName);
	void SetPSO(std::string_view psoName);

	// TODO : 동적 바인딩과 정적 바인딩을 각각 바인딩 할 수 있도록 수정필요
	void AddOverlayPass(const std::string& name, const std::string& psoName, std::vector<ShaderBinding> extraBindings, bool bInitialState = false);
	void SetOverlayPassActive(const std::string& name, bool bActive);
	void RemoveOverlayPass(const std::string& name);
	const std::vector<DirectX::BoundingBox>& GetBoundingBox() { return GetMesh()->GetBounds(); }

protected:
	void SyncMaterialSlots();

protected:
	std::vector<MaterialInstance> m_materialInstnaces = std::vector<MaterialInstance>(1);
	// 특정 오브젝트에 대해 디버깅(히트 박스 표시 등), 특수 효과(실루엣 등)를 적용하고 싶을 때 사용
	struct OverlayPass
	{
		std::string name;
		std::string psoName;
		bool bActive = false;
		std::vector<ShaderBinding> resourceBindings;
	};
	std::vector<OverlayPass> m_overlayPasses;
	std::vector<BufferHandle> m_objectCBList;//per-Submesh(material 때문에)
};

