#include "pch.h"
#include "Drawable.h"
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/UploadContext.h"

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const DrawBindingInfo& bi)
{
	if (!cmdList) return;

	if (bi.objectCBGpuVA != 0ull)
	{
		cmdList->SetGraphicsRootConstantBufferView(1, bi.objectCBGpuVA);
	}

	if (bi.indexCount > 0)
	{
		cmdList->IASetVertexBuffers(0, 1, &bi.vbv);
		cmdList->IASetIndexBuffer(&bi.ibv);
		cmdList->IASetPrimitiveTopology(bi.topology);
		cmdList->DrawIndexedInstanced(bi.indexCount, 1, 0, 0, 0);
	}
}

void DrawItem(ID3D12GraphicsCommandList* cmd, const RenderItem& item)
{
	if (!item.meshBuffer) return;

	cmd->IASetPrimitiveTopology(item.topology);

	auto vbHandle = item.meshBuffer->GetVBHandle();
	if (vbHandle.res)
	{
		D3D12_VERTEX_BUFFER_VIEW vbv{ vbHandle.res->GetGPUVirtualAddress() + vbHandle.offset, (UINT)vbHandle.size, sizeof(Vertex) };
		cmd->IASetVertexBuffers(0, 1, &vbv);
	}

	auto ibHandle = item.meshBuffer->GetIBHandle();
	if (ibHandle.res)
	{
		D3D12_INDEX_BUFFER_VIEW ibv{ ibHandle.res->GetGPUVirtualAddress() + ibHandle.offset, (UINT)ibHandle.size, DXGI_FORMAT_R32_UINT };
		cmd->IASetIndexBuffer(&ibv);
	}

	cmd->DrawIndexedInstanced(item.indexCount, item.instanceCount, item.indexOffset, item.baseVertexLocation, 0);
}