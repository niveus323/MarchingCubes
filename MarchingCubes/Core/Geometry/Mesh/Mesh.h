#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/DataStructures/Drawable.h"
#include <DirectXCollision.h>
using Microsoft::WRL::ComPtr;

class UploadContext;

class Mesh
{
public:
	explicit Mesh(UploadContext* uploadcontext, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes, const std::string& name = "");
	Mesh(UploadContext* uploadcontext, const GeometryData& data, const std::string& name = "");
	~Mesh() = default;

	void UpdateData(UploadContext* uploadcontext, const GeometryData& data);

	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_topology; }
	GeometryBuffer* GetGPUBuffer() { return &m_buffer; }
	const std::vector<MeshSubmesh>& GetSubmeshes() const { return m_submeshes; }
	size_t GetSubmeshCount() const { return m_submeshes.size(); }
	const std::vector<DirectX::BoundingBox>& GetBounds() const { return m_triBounds; }
	std::string_view GetDebugName() const { return m_debugName; }

	void SetDebugName(const std::string& name) { m_debugName = name; }
	void BuildTriBounds(const GeometryData& data);

private:
	D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	GeometryBuffer m_buffer;
	std::vector<MeshSubmesh> m_submeshes;
	std::vector<DirectX::BoundingBox> m_triBounds;
	std::string m_debugName = "";
};

