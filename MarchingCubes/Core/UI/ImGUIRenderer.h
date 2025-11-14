#pragma once
#include "Core/UI/UIRenderer.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
using Microsoft::WRL::ComPtr;

struct ImGUIInitOptions
{
	ID3D12CommandQueue* commandQueue = nullptr;
	int nums_of_frame = 1;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ComPtr<ID3D12DescriptorHeap> srvHeap; // NOTE : 정상적인 사용인지 확인할것.
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
	ImGuiConfigFlags configFlags = ImGuiConfigFlags_NavEnableKeyboard;
};

class ImGUIRenderer : public IUIRenderer
{
public:
	// IUIRenderer을(를) 통해 상속됨
	bool Initialize(const UI::InitContext& context) override;
	void RenderFrame(ID3D12GraphicsCommandList* commandList) override;
	void ShutDown() override;
	LRESULT WndMsgProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam) override;
	bool IsCapturingUI();

private:
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
};

