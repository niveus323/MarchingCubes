#pragma once
#include "Core/Utils/DXHelper.h"
#include "Win32Application.h"
#include "Core/Utils/Timer.h"
#include "Core/UI/UIRenderer.h"

class DXAppBase
{
public:
    DXAppBase(UINT width, UINT height, std::wstring name);
    virtual ~DXAppBase();

    virtual void OnInit() = 0; 
    virtual void OnInitUI() = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnRender() = 0;
    virtual void OnRenderUI() = 0;
    virtual void OnDestroy() = 0;

    void StartTimer();
    void TickAndUpdate();

    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(WPARAM key) {}
    virtual void OnKeyUp(WPARAM key) {}
    virtual void OnMouseMove(int xPos, int yPos, WPARAM buttonState) {}
    virtual void OnMouseBtnDown(int x, int y, WPARAM button) {}
    virtual void OnMouseBtnUp(int x, int y, WPARAM button) {}

    // Accessors.
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    IUIRenderer* GetUIRenderer() const { return m_uiRenderer.get(); }

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
	std::wstring GetAssetFullPath(LPCWSTR assetName);
    std::wstring GetShaderFullPath(LPCWSTR shaderName);
	
	void GetHawrdwardAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHightPerformanceAdapter = false);

	void SetCustomWindowText(LPCWSTR text);

	UINT m_width;
	UINT m_height;
	float m_aspectRatio;

	bool m_userWarpDevice;

    // UI
    std::unique_ptr<IUIRenderer> m_uiRenderer;

private:
	std::wstring m_title;
    Timer m_timer;
};

