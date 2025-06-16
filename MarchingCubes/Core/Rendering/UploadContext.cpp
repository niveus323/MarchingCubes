#include "pch.h"
#include "UploadContext.h"
#include "Core/Utils/DXHelper.h"

UploadContext::UploadContext(ID3D12Device* device) :
	m_device(device),
	m_fenceEvent(nullptr),
	m_fenceValue(1)
{
	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator)));
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_cmdList)));
	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

UploadContext::~UploadContext()
{
	CloseHandle(m_fenceEvent);
}

void UploadContext::Begin()
{
	m_allocator->Reset();
	m_cmdList->Reset(m_allocator.Get(), nullptr);
}

void UploadContext::UploadMesh(Mesh& mesh, const MeshData& data)
{
	mesh.Upload(m_device.Get(), m_cmdList.Get(), data);
}

void UploadContext::End(ID3D12CommandQueue* queue)
{
	m_cmdList->Close();
	ID3D12CommandList* lists[] = { m_cmdList.Get() };
	queue->ExecuteCommandLists(1, lists);

	queue->Signal(m_fence.Get(), m_fenceValue);
	if (m_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	m_fenceValue++;
}
