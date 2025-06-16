#include "pch.h"
#include "D3D12HelloWindow.h"

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


	D3D12HelloWindow sample(1280, 720, L"D3D12 Hello Window");	
	return Win32Application::Run(&sample, hInstance, nCmdShow);
}