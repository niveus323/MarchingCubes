#pragma once
#include "Core/UI/UIRenderer.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
using Microsoft::WRL::ComPtr;

struct ImGUIInitOptions
{
	int nums_of_frame = 1;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ComPtr<ID3D12DescriptorHeap> srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
};

class ImGUIRenderer : public IUIRenderer
{
public:
	// IUIRenderer을(를) 통해 상속됨
	bool Initialize(const UIRenderInitContext& context) override;
	void BeginFrame() override;
	void EndFrame(ID3D12GraphicsCommandList* commandList) override;
	void ShutDown() override;
	LRESULT WndMsgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
};

