#pragma once
#include "RendererComponent.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/Assets/Material/MaterialAsset.h"
#include <DirectXCollision.h>

//Forward Declaration
class StaticMesh;
class CameraComponent;

class BillboardComponent :public RendererComponent
{
	REFLECT_GENERATED_BODY(BillboardComponent)
public:
	virtual void Init() override;
	virtual void Destroy() override;
	virtual void Submit() override;
	virtual void Serialize(Serializer& ar) override;

	// Material Accessor
	void SetMaterial(std::shared_ptr<MaterialAsset> material) { m_iconMat.m_material = material; }
	void SetSize(const DirectX::XMFLOAT2& size) { m_size = size; }
	void SetIcon(std::shared_ptr<TextureAsset> textureAsset, int priority);
	
	DirectX::BoundingBox GetBoundingBox() const;
	DirectX::XMMATRIX GetWorldMatrix(CameraComponent* camera);

private:
	std::shared_ptr<StaticMesh> m_quadMesh;

	// 사용할 이미지 등 정보가 담기 MaterialInstance
	MaterialInstance m_iconMat;
	std::string m_iconIdentifier = "Default";
	DirectX::XMFLOAT2 m_size = { 1.0f, 1.0f };
	BufferHandle m_objectCB{};

	int m_priority = 0;
};