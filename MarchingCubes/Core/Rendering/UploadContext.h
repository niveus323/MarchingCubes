#pragma once
#include "Core/Geometry/Mesh.h"

class UploadContext
{
public:
	UploadContext() = default;
	~UploadContext();

	void Initailize(ID3D12Device* device);
	void Execute(ID3D12GraphicsCommandList* cmdList);
	DynamicRenderItem UploadDynamicMesh(Mesh& mesh, const MeshData& data);
	void UploadStaticMesh(ID3D12GraphicsCommandList* cmdList, Mesh& mesh, const MeshData& data);
	void UpdateMesh(Mesh& mesh);
	void UpdateMesh(Mesh& mesh, const MeshData& data);
	void SetDeletionSink(std::vector<ComPtr<ID3D12Resource>>* sink);

private:
	ComPtr<ID3D12Device> m_device;

	std::vector<Mesh*> m_meshToCommit;

	std::vector<ComPtr<ID3D12Resource>>* m_deletionSink = nullptr;
};

