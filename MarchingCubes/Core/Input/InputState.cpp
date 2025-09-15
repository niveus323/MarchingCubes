#include "pch.h"
#include "InputState.h"

InputState::InputState()
	: m_mouseX(0),
	m_mouseY(0),
	m_prevMouseX(0),
	m_prevMouseY(0),
	m_mouseDeltaX(0.0f),
	m_mouseDeltaY(0.0f),
	m_mouseInitialized(false)
{
	m_keyMap[ActionKey::Escape] = VK_ESCAPE;
	m_keyMap[ActionKey::Ctrl] = VK_CONTROL;
	m_keyMap[ActionKey::MoveForward] = 'W';
	m_keyMap[ActionKey::MoveBackward] = 'S';
	m_keyMap[ActionKey::MoveLeft] = 'A';
	m_keyMap[ActionKey::MoveRight] = 'D';
	m_keyMap[ActionKey::MoveUp] = 'E';
	m_keyMap[ActionKey::MoveDown] = 'Q';
	m_keyMap[ActionKey::ToggleDebugView] = VK_F1;
	m_keyMap[ActionKey::ToggleWireFrame] = VK_F2;
}

void InputState::OnMouseDown(int x, int y, WPARAM btn)
{
	m_mouseX = x;
	m_mouseY = y;
	
	switch (btn)
	{
	case VK_LBUTTON:
	{
		m_leftBtnState = MouseButtonState::JustPressed;
	}
		break;
	case VK_RBUTTON:
	{
		m_rightBtnState = MouseButtonState::JustPressed;
	}
		break;
	default:
		break;
	}
}

void InputState::OnMouseUp(int x, int y, WPARAM btn)
{
	m_mouseX = x;
	m_mouseY = y;

	switch (btn)
	{
	case VK_LBUTTON:
	{
		m_leftBtnState = MouseButtonState::JustReleased;
	}
	break;
	case VK_RBUTTON:
	{
		m_rightBtnState = MouseButtonState::JustReleased;
	}
	break;
	default:
		break;
	}
}

void InputState::OnMouseMove(int x, int y)
{
	m_mouseX = x;
	m_mouseY = y;

	if (!m_mouseInitialized)
	{
		m_prevMouseX = x;
		m_prevMouseY = y;
		m_mouseInitialized = true;
	}
}

void InputState::OnKeyDown(WPARAM key)
{
	m_keyState[key] = true;
}

void InputState::OnKeyUp(WPARAM key)
{
	m_keyState[key] = false;
}

bool InputState::IsPressed(ActionKey action) const
{
	auto iter = m_keyMap.find(action);
	if (iter != m_keyMap.end())
	{
		auto keyState = m_keyState.find(iter->second);
		return keyState != m_keyState.end() && keyState->second;
	}
	return false;
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

void InputState::Update()
{
	if (!m_mouseInitialized) return;

	m_mouseDeltaX = static_cast<float>(m_mouseX - m_prevMouseX);
	m_mouseDeltaY = static_cast<float>(m_prevMouseY - m_mouseY);
	m_prevMouseX = m_mouseX;
	m_prevMouseY = m_mouseY;

	switch (m_leftBtnState)
	{
	case MouseButtonState::JustPressed:
		m_leftBtnState = MouseButtonState::Pressed;
		break;
	case MouseButtonState::JustReleased:
		m_leftBtnState = MouseButtonState::NONE;
		break;
	default:
		break;
	}

	switch (m_rightBtnState)
	{
	case MouseButtonState::JustPressed:
		m_rightBtnState = MouseButtonState::Pressed;
		break;
	case MouseButtonState::JustReleased:
		m_rightBtnState = MouseButtonState::NONE;
		break;
	default:
		break;
	}
}