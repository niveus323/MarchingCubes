#pragma once
#include "App/Common/DXAppBase.h"

class DXAppBase;

class Win32Application
{
public:
	static int Run(DXAppBase* pAppBase, HINSTANCE hInstance, int nCmdShow);
	static HWND GetHwnd() { return m_hwnd; }

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam);

private:
	static HWND m_hwnd;
};

