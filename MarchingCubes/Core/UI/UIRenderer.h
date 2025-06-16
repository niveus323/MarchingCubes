#pragma once
#include <d3d12.h>
#include <any>

struct UIRenderInitContext
{
	ID3D12Device* device = nullptr;
	std::any userData = nullptr;
};

/*
* UIRenderer는 UI 프레임워크의 셋업과 라이프사이클 관리를 책임진다.
* 실제로 어떤 UI를 그리는지는 DXAppBase 및 상속 클래스의 OnUIRender() 함수에서 결정함.
*/
class IUIRenderer
{
public:
	virtual ~IUIRenderer() = default;

	virtual bool Initialize(const UIRenderInitContext& context) = 0;
	virtual void BeginFrame() = 0;
	virtual void EndFrame(ID3D12GraphicsCommandList* commandList) = 0;
	virtual void ShutDown() = 0;
	virtual LRESULT WndMsgProc(UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	std::wstring GetLastErrorMsg() const { return m_lastErrorMessage; };
protected:
	std::wstring m_lastErrorMessage;
};

