#pragma once
#include "App/Editor/Interfaces/EditorInterfaces.h"
#include "Core/Rendering/BundleRecorder.h"
#include "Core/Geometry/Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include <DirectXMath.h>

class VertexSelector : public IPickable
{
public:
	VertexSelector(ID3D12Device* device, UploadContext& meshUploader, const DirectX::XMFLOAT3& position, float radius = 0.1f);

	void SetHovered(bool bHovered) override;
	bool IsHovered() const override { return m_hovered; }
	UINT GetID() const override { return m_id; }
	bool IsPickable() const override { return true; }
	Mesh* GetMesh() const { return m_mesh.get(); }

	void RenderForPicking(ID3D12GraphicsCommandList* cmd) const override;

protected:
	DirectX::XMFLOAT4 EncodeIDColor(uint32_t id) const;

private:
	DirectX::XMFLOAT3 m_position;
	float m_radius;
	UINT m_id;
	bool m_hovered = false;
	std::shared_ptr<Mesh> m_mesh;


	// PickID Constants Buffer
	ComPtr<ID3D12Resource> m_pickIDConstantBuffer;
	UINT8* m_mappedPickIDCB;

};

