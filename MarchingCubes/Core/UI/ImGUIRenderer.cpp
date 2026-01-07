#include "pch.h"
#include "Win32Application.h"
#include "ImGUIRenderer.h"
#include "Builder/ImGUIBuilder.h"
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <algorithm>
using namespace UI;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

bool ImGUIRenderer::Initialize(const UI::InitContext& context)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	try
	{
		if (!context.userData.has_value())
		{
			m_lastErrorMessage = L"Initcontext has no value!!!!";
			return false;
		}

		if (context.userData.type() != typeid(ImGUIInitOptions))
		{
			m_lastErrorMessage = L"InitContext type invalid!!!!";
			return false;
		}

		const ImGUIInitOptions initoptions = std::any_cast<const ImGUIInitOptions&>(context.userData);
		m_srvHeap = initoptions.srvHeap;
		io.ConfigFlags = initoptions.configFlags;

		if (!ImGui_ImplWin32_Init(Win32Application::GetHwnd()))
		{
			m_lastErrorMessage = L"Failed To ImGui Win32 Initialization!!!!";
			return false;
		}

		ImGui_ImplDX12_InitInfo initInfo_dx12{};
		initInfo_dx12.Device = context.device;
		initInfo_dx12.CommandQueue = initoptions.commandQueue;
		initInfo_dx12.NumFramesInFlight = initoptions.nums_of_frame;
		initInfo_dx12.RTVFormat = initoptions.format;
		initInfo_dx12.SrvDescriptorHeap = initoptions.srvHeap.Get();
		initInfo_dx12.LegacySingleSrvCpuDescriptor = initoptions.cpuHandle;
		initInfo_dx12.LegacySingleSrvGpuDescriptor = initoptions.gpuHandle;
		if (!ImGui_ImplDX12_Init(&initInfo_dx12))
		{
			m_lastErrorMessage = L"Failed To ImGui DX12 Initialization!!!!";
			return false;
		}

		if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
	}
	catch (const std::bad_any_cast&)
	{
		m_lastErrorMessage = L"Failed to cast ImGuiInitOptions!!!!";
		return false;
	}
	catch (...)
	{
		m_lastErrorMessage = L"UnKnown Error!!!!";
		return false;
	}

	m_builder = std::make_shared<ImGUIBuilder>();
	return true;
}

void ImGUIRenderer::BeginRender()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGUIRenderer::EndRender(ID3D12GraphicsCommandList* commandList)
{
	ImGui::Render();

	ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

	// Multi-Viewport Áö¿ø
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, (void*)commandList);
	}
}

void ImGUIRenderer::ShutDown()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

LRESULT ImGUIRenderer::WndMsgProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	return false;
}
