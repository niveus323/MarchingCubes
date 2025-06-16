#pragma once
#include "Core/Geometry/Mesh.h"
class UploadContext
{
public:
	UploadContext(ID3D12Device* device);
	~UploadContext();

	void Begin();
	void UploadMesh(Mesh& mesh, const MeshData& data);
	void End(ID3D12CommandQueue* queue);

private:
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandAllocator> m_allocator;
	ComPtr<ID3D12GraphicsCommandList> m_cmdList;
	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent;
	UINT64 m_fenceValue;
};

