#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/DataStructures/Drawable.h"
#include <DirectXCollision.h>
using Microsoft::WRL::ComPtr;

class UploadContext;

class Mesh
{
public:
	explicit Mesh(UploadContext* uploadcontext, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes, std::string_view name = "");
	Mesh(UploadContext* uploadcontext, const GeometryData& data, std::string_view name = "");
	~Mesh() = default;

	void UpdateData(UploadContext* uploadcontext, const GeometryData& data);

	const GeometryData* GetCPUData() const { return &m_cpu; }
	GeometryBuffer* GetGPUBuffer() { return &m_buffer; }
	const std::vector<MeshSubmesh>& GetSubmeshes() const { return m_submeshes; }
	size_t GetSubmeshCount() const { return m_submeshes.size(); }
	const std::vector<DirectX::BoundingBox>& GetBounds() const { return m_triBounds; }
	std::string_view GetDebugName() const { return m_debugName; }

	void SetCPUData(GeometryData&& meshData) { m_cpu = std::move(meshData); }
	void SetCPUData(const GeometryData& meshData) { m_cpu = meshData; }
	void SetColor(const DirectX::XMFLOAT4& color);
	void SetDebugName(std::string_view name) { m_debugName = name; }

	void BuildTriBounds();

private:
	GeometryData m_cpu;
	GeometryBuffer m_buffer;
	std::vector<MeshSubmesh> m_submeshes;
	std::vector<DirectX::BoundingBox> m_triBounds;
	std::string m_debugName = "";
};

