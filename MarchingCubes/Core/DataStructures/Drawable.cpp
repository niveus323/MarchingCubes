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
	if (!item.meshBuffer || item.indexCount == 0) return;

	cmd->IASetPrimitiveTopology(item.topology);

	D3D12_VERTEX_BUFFER_VIEW vbv = item.meshBuffer->GetVBV();
	if (vbv.BufferLocation != 0) cmd->IASetVertexBuffers(0, 1, &vbv);

	D3D12_INDEX_BUFFER_VIEW ibv = item.meshBuffer->GetIBV();
	if (ibv.BufferLocation != 0) cmd->IASetIndexBuffer(&ibv);

	for (const auto& binding : item.resourceBindings)
	{
		switch (binding.type)
		{
			case EBindingType::CONSTANTS:
				cmd->SetGraphicsRoot32BitConstant(binding.rootParameterIndex, binding.constantData, 0);
				break;
			case EBindingType::CBV:
				cmd->SetGraphicsRootConstantBufferView(binding.rootParameterIndex, binding.gpuAddress);
				break;
			case EBindingType::SRV:
				cmd->SetGraphicsRootShaderResourceView(binding.rootParameterIndex, binding.gpuAddress);
				break;
			case EBindingType::UAV:
				cmd->SetGraphicsRootUnorderedAccessView(binding.rootParameterIndex, binding.gpuAddress);
				break;
			case EBindingType::TABLE:
				cmd->SetGraphicsRootDescriptorTable(binding.rootParameterIndex, binding.gpuDescriptorHandle);
				break;
			default:
				Log::Print("RenderItem", "Invalid Binding Type!!!!\n Check For: %s", item.debugName.c_str());
				break;
		}
	}

	cmd->DrawIndexedInstanced(item.indexCount, item.instanceCount, item.indexOffset, static_cast<INT>(item.baseVertexLocation), 0);

}