#include "pch.h"
#include "UploadContext.h"

UploadContext::~UploadContext()
{
	m_device.Reset();
}

void UploadContext::Initailize(ID3D12Device* device)
{
	m_device = device;
	NAME_D3D12_OBJECT(m_device);
}

void UploadContext::Execute(ID3D12GraphicsCommandList* cmdList)
{
#ifdef _DEBUG
	assert(m_device.Get() && "Init() must be called before Execute()");
#endif

	for (Mesh* mesh : m_meshToCommit)
	{
		if (m_deletionSink) mesh->SetDeletionSink(m_deletionSink);

		mesh->CommitBuffers(m_device.Get(), cmdList);
	}
	m_meshToCommit.clear();
}

DynamicRenderItem UploadContext::UploadDynamicMesh(Mesh& mesh, ID3D12Fence* graphicsFence, const MeshData& data)
{
	mesh.Initialize(m_device.Get(), data);
	mesh.StageBuffers(m_device.Get(), graphicsFence);
	if (std::find(m_meshToCommit.begin(), m_meshToCommit.end(), &mesh) == m_meshToCommit.end())
	{
		m_meshToCommit.push_back(&mesh);
	}

	DynamicRenderItem item;
	item.object = &mesh;

	return item;
}

void UploadContext::UploadStaticMesh(ID3D12Fence* graphicsFence, ID3D12GraphicsCommandList* cmdList, Mesh& mesh, const MeshData& data)
{
	mesh.Initialize(m_device.Get(), data);
	mesh.StageBuffers(m_device.Get(), graphicsFence);
	mesh.CommitBuffers(m_device.Get(), cmdList);
}

void UploadContext::UpdateMesh(Mesh& mesh)
{
	//mesh.StageBuffers();
	mesh.BuildTriBounds();
	if (std::find(m_meshToCommit.begin(), m_meshToCommit.end(), &mesh) == m_meshToCommit.end())
	{
		m_meshToCommit.push_back(&mesh);
	}
}

void UploadContext::UpdateMesh(ID3D12Fence* graphicsFence, Mesh& mesh, const MeshData& data)
{
	mesh.UpdateData(m_device.Get(), graphicsFence, data);
	if (std::find(m_meshToCommit.begin(), m_meshToCommit.end(), &mesh) == m_meshToCommit.end())
	{
		m_meshToCommit.push_back(&mesh);
	}

}

void UploadContext::SetDeletionSink(std::vector<ComPtr<ID3D12Resource>>* sink)
{
	m_deletionSink = sink;
}
