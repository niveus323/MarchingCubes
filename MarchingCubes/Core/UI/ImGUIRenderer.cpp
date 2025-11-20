#include "pch.h"
#include "ImGUIRenderer.h"
#include "Win32Application.h"
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

	return true;
}

void ImGUIRenderer::RenderFrame(ID3D12GraphicsCommandList* commandList)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	uint64_t now = Timer::GetTimeMs();
	std::vector<std::shared_ptr<UIEntry>> copy;
	{
		std::lock_guard<std::mutex> lock(m_entriesMutex);
		copy = m_entries;
	}

	std::sort(copy.begin(), copy.end(), [](auto const& a, auto const& b) { return a->priority > b->priority; });
	for (auto& entry : copy)
	{
		if (!entry->enabled.load(std::memory_order_relaxed)) continue;
		if (entry->rateHz > 0)
		{
			uint64_t interval = 1000u / (uint64_t)entry->rateHz;
			if (now - entry->lastTimestamp < interval) continue;
			entry->lastTimestamp = now;
		}

		try
		{
			entry->callback();
		}
		catch (...)
		{
			Log::Print("ImGUIRenderer", "Callback Failed!!!!");
		}
	}

	ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

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

bool ImGUIRenderer::IsCapturingUI()
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
}

