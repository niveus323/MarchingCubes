#include "pch.h"
#include "App/Editor/Interface/EditorApp.h"
#include <dxgidebug.h>


#ifdef _DEBUG
void CreateConsole()
{
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
}
#endif // _DEBUG

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
#ifdef _DEBUG
	CreateConsole();
#endif // _DEBUG

    int exitCode = 0;
    {
        EditorApp sample(1280, 720, L"Editor");
        exitCode = Win32Application::Run(&sample, hInstance, nCmdShow);
    }

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
#endif

    return exitCode;
}