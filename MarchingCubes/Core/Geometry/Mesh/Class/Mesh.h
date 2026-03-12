#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/DataStructures/Drawable.h"
#include <DirectXCollision.h>
using Microsoft::WRL::ComPtr;

class UploadContext;

class Mesh
{
public:
	Mesh(const std::string& name = "");
	~Mesh() = default;
	void Initialize(const GeometryBuffer& buffer, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes);

	D3D_PRIMITIVE_TOPOLOGY GetTopology() const { return m_topology; }
	GeometryBuffer* GetGPUBuffer() { return &m_buffer; }
	const std::vector<MeshSubmesh>& GetSubmeshes() const { return m_submeshes; }
	size_t GetSubmeshCount() const { return m_submeshes.size(); }
	const std::vector<DirectX::BoundingBox>& GetBounds() const { return m_triBounds; }
	std::string_view GetDebugName() const { return m_debugName; }
	void SetDebugName(const std::string& name) { m_debugName = name; }
	
	// TODO : _DEBUG -> _EDITOR로 변경
#ifdef _DEBUG
	const GeometryData& GetGeometryData() const { return m_cpuData; }
#endif

protected:
	void BuildTriBounds(const GeometryData& data);
	
#ifdef _DEBUG
	GeometryData m_cpuData; // 에디터 씬 RenderTarget 구현 전까지 임시로 사용
#endif // _DEBUG

protected:
	D3D_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	GeometryBuffer m_buffer;
	std::vector<MeshSubmesh> m_submeshes;
	std::vector<DirectX::BoundingBox> m_triBounds;
	std::string m_debugName = "";
};

