#pragma once
#include "Core/UI/Renderer/UIRenderer.h"
#include "Core/UI/Builder/ImGuiBuilder.h"
#include <imgui.h>
using Microsoft::WRL::ComPtr;

struct ImGUIInitOptions
{
	ID3D12CommandQueue* commandQueue = nullptr;
	int nums_of_frame = 1;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ComPtr<ID3D12DescriptorHeap> srvHeap; // NOTE : 정상적인 사용인지 확인할것.
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
	ImGuiConfigFlags configFlags = ImGuiConfigFlags_None;
};

class ImGUIRenderer : public IUIRenderer
{
public:
	bool Initialize(const UI::InitContext& context) override;
	void BeginRender() override;
	void EndRender(ID3D12GraphicsCommandList* commandList) override;
	void ShutDown() override;
	LRESULT WndMsgProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam) override;
	bool IsCapturingUI() override { return IsCapturingMouse() || IsCapturingKeyboard(); }
	bool IsCapturingMouse() override { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse; }
	bool IsCapturingKeyboard() override { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard; }

private:
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
};

