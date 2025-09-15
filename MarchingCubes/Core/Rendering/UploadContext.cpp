#include "pch.h"
#include "UploadContext.h"

UploadContext::~UploadContext()
{
	m_device.Reset();
}

void UploadContext::Initailize(ID3D12Device* device)
{
	m_device = device;
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

DynamicRenderItem UploadContext::UploadDynamicMesh(Mesh& mesh, const MeshData& data)
{
	mesh.Initialize(m_device.Get(), data);
	mesh.StageBuffers();
	if (std::find(m_meshToCommit.begin(), m_meshToCommit.end(), &mesh) == m_meshToCommit.end())
	{
		m_meshToCommit.push_back(&mesh);
	}

	DynamicRenderItem item;
	item.object = &mesh;

	return item;
}

void UploadContext::UploadStaticMesh(ID3D12GraphicsCommandList* cmdList, Mesh& mesh, const MeshData& data)
{
	mesh.Initialize(m_device.Get(), data);
	mesh.StageBuffers();
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

void UploadContext::UpdateMesh(Mesh& mesh, const MeshData& data)
{
	mesh.UpdateData(data);
	if (std::find(m_meshToCommit.begin(), m_meshToCommit.end(), &mesh) == m_meshToCommit.end())
	{
		m_meshToCommit.push_back(&mesh);
	}

}

void UploadContext::SetDeletionSink(std::vector<ComPtr<ID3D12Resource>>* sink)
{
	m_deletionSink = sink;
}
