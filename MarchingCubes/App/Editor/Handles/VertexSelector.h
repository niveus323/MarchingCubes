#pragma once
#include "App/Editor/Interfaces/EditorInterfaces.h"
#include "Core/Rendering/BundleRecorder.h"
#include "Core/Geometry/Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include <DirectXMath.h>

class VertexSelector : public IPickable
{
public:
	VertexSelector(ID3D12Device* device, UploadContext& meshUploader, const DirectX::XMFLOAT3& position, int id, float radius = 0.1f, const DirectX::XMFLOAT4& defaultColor = { 1,1,1,1 }, const DirectX::XMFLOAT4& selectedColor = { 1,0,0,0 });
	~VertexSelector();

	void SetHovered(bool bHovered) override;
	bool IsHovered() const override { return m_hovered; }
	UINT GetID() const override { return m_id; }
	bool IsPickable() const override { return true; }
	Mesh* GetMesh() const override { return m_mesh.get(); }
	void SetSelected(bool bSelected) override;
	bool IsSelected() const { return m_selected; }
	const DynamicRenderItem& GetRenderItem() const { return m_renderItem; }

protected:
	DirectX::XMFLOAT4 EncodeIDColor(uint32_t id) const;

private:
	DirectX::XMFLOAT3 m_position;
	float m_radius;
	UINT m_id;
	bool m_hovered = false;
	bool m_selected = false;
	std::unique_ptr<Mesh> m_mesh;
	DirectX::XMFLOAT4 m_defaultcolor = { 1,1,1,1 };
	DirectX::XMFLOAT4 m_selectedColor = { 1,0,0,0 };
	DynamicRenderItem m_renderItem;
};

