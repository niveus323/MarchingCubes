#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/DataStructures/ShaderTypes.h"
#include "Core/DataStructures/Drawable.h"
#include "Core/Rendering/Material.h"
#include <Core/Rendering/Camera.h>
#include <DirectXCollision.h>
using Microsoft::WRL::ComPtr;

class Mesh :public IDrawable
{
public:
	Mesh(ID3D12Device* device, const GeometryData& data);
	Mesh(ID3D12Device* device, const GeometryData& data, const ObjectConstants& cb, std::shared_ptr<Material> material);

	// IDrawable을(를) 통해 상속됨
	DrawBindingInfo GetDrawBinding() const override;
	ObjectConstants GetObjectConstants() const override { return m_objectCB; }
	void Update(float deltaTime);
	void UpdateConstants();
	
	const GeometryData* GetCPUData() const override { return &m_cpu; }
	GeometryBuffer* GetGPUBuffer() override { return &m_buffer; }
	void SetUploadPending(bool pending) override { m_needsUpload = pending; }
	bool IsUploadPending() const override { return m_needsUpload; }
	const char* GetDebugName() const override { return m_debugName.c_str(); }
	void SetDebugName(const std::string& name) override { m_debugName = name; }
	const std::vector<DirectX::BoundingBox>& GetBounds() const { return m_triBounds; }
	DirectX::XMMATRIX GetWorldMatrix() const;
	DirectX::XMMATRIX GetWorldInvMatrix() const;

	void SetCPUData(const GeometryData& meshData) override { m_cpu = meshData; }
	void SetCPUData(GeometryData&& meshData) override { m_cpu = std::move(meshData); }
	Material* GetMaterial() { return m_material.get(); }
	void SetMaterial(std::shared_ptr<Material> mat) { m_material = std::move(mat); }

	DirectX::XMFLOAT3 GetPosition() const { return m_position; }
	void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }
	DirectX::XMFLOAT3 GetRotation() const { return m_rotation; }
	void SetRotation(const DirectX::XMFLOAT3& rotation) { m_rotation = rotation; }
	void SetRotation(const DirectX::XMVECTOR& quat) { m_rotation = ToEulerFromQuat(quat); }
	DirectX::XMFLOAT3 GetScale() const { return m_scale; }
	void SetScale(const DirectX::XMFLOAT3& scale) { m_scale = scale; }
	void SetColor(const DirectX::XMFLOAT4& color);

	void Move(const DirectX::XMFLOAT3& delta);
	void Rotate(const DirectX::XMVECTOR& deltaQuat);
	void Scale(const DirectX::XMFLOAT4& scaleFactor);
	void BuildTriBounds();

private:
	GeometryBuffer m_buffer;
	GeometryData m_cpu;
	ObjectConstants m_objectCB{};

	std::shared_ptr<Material> m_material; // Material은 App 클래스에서 공유받아 bind만 해준다.

	DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };

	std::vector<DirectX::BoundingBox> m_triBounds;

	bool m_needsUpload = false;
	std::string m_debugName = "";
};

