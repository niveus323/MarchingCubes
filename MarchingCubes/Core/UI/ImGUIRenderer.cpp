#include "pch.h"
#include "ImGUIRenderer.h"
#include "Win32Application.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool ImGUIRenderer::Initialize(const UIRenderInitContext& context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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

        if (!ImGui_ImplWin32_Init(Win32Application::GetHwnd()))
        {
            m_lastErrorMessage = L"Failed To ImGui Win32 Initialization!!!!";
            return false;
        }
        if (!ImGui_ImplDX12_Init(
            context.device,
            initoptions.nums_of_frame,
            initoptions.format,
            initoptions.srvHeap.Get(),
            initoptions.cpuHandle,
            initoptions.gpuHandle
        ))
        {
            m_lastErrorMessage = L"Failed To ImGui DX12 Initialization!!!!";
            return false;
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

void ImGUIRenderer::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGUIRenderer::EndFrame(ID3D12GraphicsCommandList* commandList)
{
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void ImGUIRenderer::ShutDown()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

LRESULT ImGUIRenderer::WndMsgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(Win32Application::GetHwnd(), msg, wParam, lParam))
        return true;

    return false;
}
