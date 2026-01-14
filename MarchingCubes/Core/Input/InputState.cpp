#include "pch.h"
#include "InputState.h"

InputState::InputState()
{
	m_keyMap[ActionKey::Escape] = VK_ESCAPE;
	m_keyMap[ActionKey::Ctrl] = VK_CONTROL;
	m_keyMap[ActionKey::LeftClick] = VK_LBUTTON;
	m_keyMap[ActionKey::MiddleClick] = VK_MBUTTON;
	m_keyMap[ActionKey::RightClick] = VK_RBUTTON;
	m_keyMap[ActionKey::MoveForward] = 'W';
	m_keyMap[ActionKey::MoveBackward] = 'S';
	m_keyMap[ActionKey::MoveLeft] = 'A';
	m_keyMap[ActionKey::MoveRight] = 'D';
	m_keyMap[ActionKey::MoveUp] = 'E';
	m_keyMap[ActionKey::MoveDown] = 'Q';
	m_keyMap[ActionKey::ToggleDebugView] = VK_F1;
	m_keyMap[ActionKey::ToggleWireFrame] = VK_F2;
	m_keyMap[ActionKey::ToggleDebugNormal] = VK_F3;
}

void InputState::Update()
{
	if (!m_mouseInitialized) return;

	MousePos delta = m_curPos - m_prevPos;
	m_prevPos = m_curPos;

	if (m_isMouseCaptured) m_deltaPos = { 0, 0 };
	else m_deltaPos = delta;

	for (auto& state : m_keyState)
	{
		switch (state.second)
		{
			case ActionKeyState::JustPressed:
				state.second = ActionKeyState::Pressed;
				break;
			case ActionKeyState::JustReleased:
				state.second = ActionKeyState::NONE;
				break;
			default:
				break;
		}
	}
}

void InputState::OnMouseMove(int x, int y)
{
	m_curPos = MousePos(x, y);

	if (!m_mouseInitialized)
	{
		m_prevPos = MousePos(x, y);
		m_mouseInitialized = true;
	}
}

bool InputState::IsPressed(ActionKey action) const
{
	ActionKeyState state = GetKeyState(action);
	return state == ActionKeyState::JustPressed || state == ActionKeyState::Pressed;
}

float InputState::GetAxisValue(ActionKey action) const
{
	if (action == ActionKey::MouseX) return static_cast<float>(m_deltaPos.x);
	if (action == ActionKey::MouseY) return static_cast<float>(m_deltaPos.y);

	if (IsPressed(action)) return 1.0f;

	return 0.0f;
}

ActionKeyState InputState::GetKeyState(ActionKey action) const
{
	bool isMouseKey = (action == ActionKey::LeftClick || action == ActionKey::RightClick || action == ActionKey::MiddleClick);
	if (isMouseKey && m_isMouseCaptured) return ActionKeyState::NONE;
	if (!isMouseKey && m_isKeyboardCaptured) return ActionKeyState::NONE;

	auto iter = m_keyMap.find(action);
	if (iter != m_keyMap.end())
	{
		auto keyState = m_keyState.find(iter->second);
		if (keyState == m_keyState.end()) 
			return ActionKeyState::NONE;
		
		return keyState->second;
	}

	return ActionKeyState::NONE;
}

// TODO : 키 매핑 커스터마이징 기능 추가 (특수 키를 위한 문자열 <-> VK 매핑 테이블 추가)
void InputState::LoadKeyBindingsFromIni(const std::wstring& filename)
{
	const wchar_t* section = L"KeyBindings";
	for (const auto& pair : m_keyMap)
	{
		ActionKey action = pair.first;
		const wchar_t* keyName = nullptr;
		switch (action)
		{
		case ActionKey::Escape:
			keyName = L"Escape";
			break;

		case ActionKey::MoveForward:
			keyName = L"MoveForward";
			break;

		case ActionKey::MoveBackward:
			keyName = L"MoveBackward";
			break;

		case ActionKey::MoveLeft:
			keyName = L"MoveLeft";
			break;
			
		case ActionKey::MoveRight:
			keyName = L"MoveRight";
			break;
		
		case ActionKey::MoveUp:
			keyName = L"MoveUp";
			break;

		case ActionKey::MoveDown:
			keyName = L"MoveDown";
			break;
		
		default:
			continue;
		}

		wchar_t value[16] = {  };
		GetPrivateProfileString(section, keyName, L"", value, 16, filename.c_str());

		if (value[0] != 0)
		{
			m_keyMap[action] = static_cast<WPARAM>(toupper(value[0]));
		}
	}

}

void InputState::SaveKeyBindingsToIni(const std::wstring& filename)
{
	const wchar_t* section = L"KeyBindings";
	for (const auto& iter : m_keyMap)
	{
		const wchar_t* keyName = nullptr;
		switch (iter.first) {
		case ActionKey::Escape:		  keyName = L"Escape"; break;
		case ActionKey::MoveForward:  keyName = L"MoveForward"; break;
		case ActionKey::MoveBackward: keyName = L"MoveBackward"; break;
		case ActionKey::MoveLeft:     keyName = L"MoveLeft"; break;
		case ActionKey::MoveRight:    keyName = L"MoveRight"; break;
		case ActionKey::MoveUp:		  keyName = L"MoveUp"; break;
		case ActionKey::MoveDown:	  keyName = L"MoveDown"; break;

		default: continue;
		}

		wchar_t value[2] = { wchar_t(iter.second), 0 };
		WritePrivateProfileStringW(section, keyName, value, filename.c_str());
	}

}