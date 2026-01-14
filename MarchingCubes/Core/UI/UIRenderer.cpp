#include "pch.h"
#include "UIRenderer.h"
#include "Core/Utils/Timer.h"

void IUIRenderer::RenderFrame(ID3D12GraphicsCommandList* commandList)
{
	BeginRender();

	uint64_t now = Timer::GetTimeMs();
	std::vector<std::shared_ptr<UIEntry>> renderEntries;
	{
		std::lock_guard<std::mutex> lock(m_entriesMutex);

		// 정렬 요청이 있었으면 수행
		if (m_needSort)
		{
			SortEntries();
			m_needSort = false;
		}

		renderEntries = m_entries;
	}
    
	for (auto& entry : renderEntries)
	{
		if (!entry->enabled.load(std::memory_order_relaxed)) continue;

		// Hz(Refresh Rate) 제한 체크
		if (entry->rateHz > 0)
		{
			uint64_t interval = 1000u / (uint64_t)entry->rateHz;
			if (now - entry->lastTimestamp < interval) continue;
			entry->lastTimestamp = now;
		}

		if (m_builder)
		{
			entry->callback(m_builder.get());
		}
	}

	EndRender(commandList);
}

UI::FrameCallbackToken IUIRenderer::AddFrameRenderCallbackToken(UI::FrameRenderCallback callback, UI::UICallbackOptions options)
{
	auto ent = std::make_shared<UIEntry>();
	ent->token = m_nextToken++;

	std::weak_ptr<UIEntry> weakEnt = ent;
	ent->callback = [this, userCallback = std::move(callback), weakEnt](IUIBuilder* builder)
		{
			// 실제 그리기
			userCallback(builder);

			if (auto entPtr = weakEnt.lock())
			{
				bool isFocused = builder->IsWindowFocused();
				if (isFocused && !entPtr->wasFocused)
				{
					this->RequestFocus(entPtr->token);
				}
				entPtr->wasFocused = isFocused;
			}
		};
	ent->layer = options.layer;
	ent->enabled.store(options.enabled);
	ent->rateHz = options.rateHz;
	ent->id = options.id;
	ent->lastTimestamp = 0;

	std::lock_guard<std::mutex> lock(m_entriesMutex);
	m_entries.push_back(ent);

	SortEntries();
	return ent->token;
}

void IUIRenderer::RemoveFrameRenderCallback(UI::FrameCallbackToken token)
{
	std::lock_guard<std::mutex> lock(m_entriesMutex);
	m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](auto& e) { return e->token == token; }), m_entries.end());
}

void IUIRenderer::SetCallbackEnabled(UI::FrameCallbackToken token, bool enabled)
{
	std::lock_guard<std::mutex> lock(m_entriesMutex);
	for (auto& e : m_entries) 
	{ 
		if (e->token == token) e->enabled = enabled; 
	}
}

void IUIRenderer::SetCallbackRate(UI::FrameCallbackToken token, int hz)
{
	std::lock_guard<std::mutex> lock(m_entriesMutex);
	for (auto& e : m_entries) 
	{ 
		if (e->token == token) e->rateHz = hz; 
	}
}

void IUIRenderer::SetCallbackLayer(UI::FrameCallbackToken token, UI::EUILayer newLayer)
{
	std::lock_guard<std::mutex> lock(m_entriesMutex);
	bool found = false;
	for (auto& entry : m_entries)
	{
		if (entry->token == token)
		{
			entry->layer = newLayer;
			found = true;
			break;
		}
	}
	if (found) m_needSort = true;
}

void IUIRenderer::RequestFocus(UI::FrameCallbackToken token)
{
	std::lock_guard<std::mutex> lock(m_entriesMutex);

	auto it = std::find_if(m_entries.begin(), m_entries.end(), [token](const auto& entry) { 
		return entry->token == token; 
	});

	if (it != m_entries.end())
	{
		// 현재 가장 높은 순서 번호를 부여
		(*it)->lastFocusedOrder = ++m_focusOrderCounter;
		m_needSort = true;
	}
}

void IUIRenderer::SortEntries()
{
	std::sort(m_entries.begin(), m_entries.end(), [](const std::shared_ptr<UIEntry>& a, const std::shared_ptr<UIEntry>& b){
		if (a->layer != b->layer) return a->layer < b->layer;

		return a->lastFocusedOrder < b->lastFocusedOrder;
	});
}
