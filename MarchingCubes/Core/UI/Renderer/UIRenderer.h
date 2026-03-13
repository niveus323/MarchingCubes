#pragma once
#include "Core/UI/Builder/UIBuilder.h"
#include <d3d12.h>
#include <any>
#include <functional>
#include <memory>
#include <mutex>

// Forward Declaration
class IUIBuilder;

namespace UI
{
	using FrameCallbackToken = size_t;
	using FrameRenderCallback = std::function<void(IUIBuilder*)>;

	struct InitContext
	{
		ID3D12Device* device = nullptr;
		std::any userData = nullptr;
	};

	/*
	* 낮은 값이 먼저 그려짐 (배경) -> 높은 값이 나중에 그려짐 (맨 위)
	* [0 ~ 19] World Zone (3D 씬과 직접 연관된 요소)
	* [20 ~ 99] Game UI Zone (실제 게임 플레이 UI)
	* [100 ~ 199] Editor UI Zone (엔진 툴 UI)
	* [200 ~ 255] Global / System Zone (최상위 시스템 오버레이)
	*/
	enum class EUILayer : uint8_t
	{
		None = 0, // 렌더링 x, 초기화 전용
		World_Viewport = 1, // 3D Viewport 배경
		World_Floating = 10, // 3D 오브젝트 위에 그려지지만 다른 UI보다는 뒤에 있음
		Game_Screen = 20, // 게임 화면 전체에 깔리는 효과
		Game_HUD = 30, // 게임 내 항상 떠 있는 정보
		Game_Window = 40, // 게임 내 일반 창
		Game_Popup = 50, // 게임 내 팝업
		Game_Cinematic = 60, // 최상위 게임 연출
		Game_Menu = 70, // 게임 내 전체 메뉴
		Editor_Background = 100, // 에디터 배경
		Editor_Panel = 110, // 에디터 UI 패널
		Editor_Window = 120, // 에디터 독립 창 (설정 등)
		Editor_Popup = 130, // 에디터 팝업
		Global_Tooltip = 200, // 최상위 툴팁
		Global_Context = 210, // 우클릭 메뉴
		Global_DragDrop = 220, // 드래그& 드랍 아이콘
		Global_Notification = 230, // 시스템 알림
		Global_Debug = 250, // 디버그 정보
		Global_Cursor = 255 // 소프트웨어 마우스 커서
	};

	/*
	* UI 렌더는 콜백 함수 등록으로 화면에 어떻게 렌더링 할 것인지를 결정하도록 한다.
	*/
	struct UICallbackOptions
	{
		EUILayer layer = EUILayer::None;
		int rateHz = 0;
		bool enabled = true;
		std::string id;
	};
}

/*
* UIRenderer는 UI 프레임워크의 셋업과 라이프사이클 관리를 책임진다.
* 실제로 어떤 UI를 그리는지는 DXAppBase 및 상속 클래스의 OnUIRender() 함수에서 결정함.
*/
class IUIRenderer
{
public:
	virtual ~IUIRenderer() = default;

	virtual bool Initialize(const UI::InitContext & context) = 0;
	virtual void RenderFrame(ID3D12GraphicsCommandList* commandList);
	virtual void BeginRender() = 0;
	virtual void EndRender(ID3D12GraphicsCommandList* commandList) = 0;
	virtual void ShutDown() = 0;
	virtual LRESULT WndMsgProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam) = 0;
	virtual bool IsCapturingUI() = 0;
	virtual bool IsCapturingMouse() = 0;
	virtual bool IsCapturingKeyboard() = 0;
	virtual bool IsMouseInteracting() = 0;
	virtual bool IsKeyboardInteracting() = 0;
	std::wstring GetLastErrorMsg() const { return m_lastErrorMessage; };

	UI::FrameCallbackToken AddFrameRenderCallbackToken(UI::FrameRenderCallback callback, UI::UICallbackOptions options = {});
	void RemoveFrameRenderCallback(UI::FrameCallbackToken token);
	void SetCallbackEnabled(UI::FrameCallbackToken token, bool enabled);
	void SetCallbackRate(UI::FrameCallbackToken token, int hz);
	void SetCallbackLayer(UI::FrameCallbackToken token, UI::EUILayer newLayer);
	void RequestFocus(UI::FrameCallbackToken token);

protected:
	void SortEntries();

protected:
	std::shared_ptr<IUIBuilder> m_builder;
	std::wstring m_lastErrorMessage;
	struct UIEntry
	{
		UI::FrameCallbackToken token = 0;
		UI::FrameRenderCallback callback = nullptr;
		UI::EUILayer layer = UI::EUILayer::None;
		uint64_t lastFocusedOrder = 0;
		std::atomic<bool> enabled = false;
		int rateHz = 0;
		uint64_t lastTimestamp = 0ull;
		std::string id = "";
		bool wasFocused = false;
	};
	std::vector<std::shared_ptr<UIEntry>> m_entries;
	std::mutex m_entriesMutex;
	UI::FrameCallbackToken m_nextToken = 1;
	uint64_t m_focusOrderCounter = 0;
	bool m_needSort = false; // UI 정렬 Dirty Flag
};

