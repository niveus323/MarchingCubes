#include "pch.h"
#include "Win32Application.h"
#include "Core/UI/UIRenderer.h"
HWND Win32Application::m_hwnd = nullptr;

int Win32Application::Run(DXAppBase* pAppBase, HINSTANCE hInstance, int nCmdShow)
{
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
	pAppBase->ParseCommandLineArgs(argv, argc);

    //CommandLineToArgvW를 통해 얻은 argv 객체는 LocalFree로 해제해야함. (https://learn.microsoft.com/ko-kr/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw)
	LocalFree(argv);
	
    // CreateUploadBuffer the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DXSampleClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(pAppBase->GetWidth()), static_cast<LONG>(pAppBase->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        pAppBase->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        pAppBase);

    // CreateUploadBuffer the sample. OnInit is defined in each child-implementation of DXSample.
    pAppBase->OnInit();
    pAppBase->StartTimer();

    ShowWindow(m_hwnd, nCmdShow);

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            pAppBase->TickAndUpdate();
            pAppBase->OnRender();
        }
       
    }

    pAppBase->OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

LRESULT Win32Application::WindowProc(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        // Save the DXSample* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        return 0;
    }
    
    if(message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    if (auto* pAppBase = reinterpret_cast<DXAppBase*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
        if (auto* ui = pAppBase->GetUIRenderer()) {
            if (ui->WndMsgProc(hWnd, message, wParam, lParam))
                return 0;
        }

        // 게임용 메시지 처리
        switch (message)
        {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_INPUT:
            {
                pAppBase->OnPlatformEvent(message, wParam, lParam);
                return 0;
            }
        }
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
