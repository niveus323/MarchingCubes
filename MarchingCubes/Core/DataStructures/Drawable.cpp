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