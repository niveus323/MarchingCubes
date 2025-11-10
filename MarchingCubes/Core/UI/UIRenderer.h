#pragma once
#include <d3d12.h>
#include <any>
#include <functional>
#include <memory>
#include <mutex>

namespace UI
{
	using FrameCallbackToken = size_t;
	using FrameRenderCallback = std::function<void()>;

	struct InitContext
	{
		ID3D12Device* device = nullptr;
		std::any userData = nullptr;
	};

	/*
	* UI 렌더는 콜백 함수 등록으로 화면에 어떻게 렌더링 할 것인지를 결정하도록 한다.
	*/
	struct UICallbackOptions
	{
		int priority = 0;
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
	virtual void BeginFrame() = 0;
	virtual void EndFrame(ID3D12GraphicsCommandList* commandList) = 0;
	virtual void RenderFrame(ID3D12GraphicsCommandList* commandList) = 0;
	virtual void ShutDown() = 0;
	virtual LRESULT WndMsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	virtual bool IsCapturingUI() = 0;
	std::wstring GetLastErrorMsg() const { return m_lastErrorMessage; };

	UI::FrameCallbackToken AddFrameRenderCallbackToken(UI::FrameRenderCallback callback, UI::UICallbackOptions options = {}) 
	{
		auto ent = std::make_shared<UIEntry>();
		ent->token = m_nextToken++;
		ent->callback = std::move(callback);
		ent->priority = options.priority;
		ent->enabled.store(options.enabled);
		ent->rateHz = options.rateHz;
		ent->id = options.id;
		ent->lastTimestamp = 0;

		std::lock_guard<std::mutex> lock(m_entriesMutex);
		m_entries.push_back(ent);
		return ent->token;
	}
	void RemoveFrameRenderCallback(UI::FrameCallbackToken token) 
	{
		std::lock_guard<std::mutex> lock(m_entriesMutex);
		m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](auto& e) { return e->token == token; }), m_entries.end());
	}
	void SetCallbackEnabled(UI::FrameCallbackToken token, bool enabled)
	{
		std::lock_guard<std::mutex> lock(m_entriesMutex);
		for (auto& iter = m_entries.begin(); iter != m_entries.end(); ++iter)
		{
			if (iter->get()->token == token) iter->get()->enabled = enabled;
		}
	}
	void SetCallbackRate(UI::FrameCallbackToken token, int hz)
	{
		std::lock_guard<std::mutex> lock(m_entriesMutex);
		for (auto& iter = m_entries.begin(); iter != m_entries.end(); ++iter)
		{
			if (iter->get()->token == token) iter->get()->rateHz = hz;
		}
	}

protected:
	std::wstring m_lastErrorMessage;
	struct UIEntry
	{
		UI::FrameCallbackToken token;
		UI::FrameRenderCallback callback;
		int priority;
		std::atomic<bool> enabled;
		int rateHz;
		uint64_t lastTimestamp;
		std::string id;
	};
	std::vector<std::shared_ptr<UIEntry>> m_entries;
	std::mutex m_entriesMutex;
	UI::FrameCallbackToken m_nextToken = 1;
};

