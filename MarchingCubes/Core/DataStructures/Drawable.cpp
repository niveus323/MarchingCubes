#include "pch.h"
#include "Drawable.h"
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/UploadContext.h"

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const DrawBindingInfo& bi)
{
	if (!cmdList) return;

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

	if (item.indexCount == 0)
	{
		//Log::Print("RenderSystem", "%s : Index Count is Zero", item.debugName.c_str());
		return;
	}

	cmd->IASetPrimitiveTopology(item.topology);

	D3D12_VERTEX_BUFFER_VIEW vbv = item.meshBuffer->GetVBV();
	if (vbv.BufferLocation != 0) cmd->IASetVertexBuffers(0, 1, &vbv);

	D3D12_INDEX_BUFFER_VIEW ibv = item.meshBuffer->GetIBV();
	if (ibv.BufferLocation != 0) cmd->IASetIndexBuffer(&ibv);

	for (const auto& binding : item.resourceBindings)
	{
		switch (binding.type)
		{
			case EBindingType::CBV:
				cmd->SetGraphicsRootConstantBufferView(binding.rootParameterIndex, binding.gpuAddress);
				break;
			case EBindingType::SRV:
			case EBindingType::UAV:
				cmd->SetGraphicsRootDescriptorTable(binding.rootParameterIndex, binding.gpuDescriptorHandle);
				break;
			default:
				Log::Print("RednerItem", "Invalid ExtraBinding!!!!\n Check For: %s", item.debugName);
				break;
		}
	}

	cmd->DrawIndexedInstanced(item.indexCount, item.instanceCount, item.indexOffset, static_cast<INT>(item.baseVertexLocation), 0);

}