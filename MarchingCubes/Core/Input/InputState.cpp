#include "pch.h"
#include "InputState.h"

InputState::InputState()
{
	// --- Common ---
	m_actionMap[ActionKey::Escape] = {
		.keyCode = VK_ESCAPE,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::Ctrl] = {
		.keyCode = VK_CONTROL,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::Shift] = {
		.keyCode = VK_SHIFT,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::LeftClick] = {
		.keyCode = VK_LBUTTON,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MiddleClick] = {
		.keyCode = VK_MBUTTON,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::RightClick] = {
		.keyCode = VK_RBUTTON,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::ToggleDebugView] = {
		.keyCode = VK_F1,
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MoveForward] = {
		.keyCode = 'W',
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MoveLeft] = {
		.keyCode = 'A',
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MoveBackward] = {
		.keyCode = 'S',
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MoveRight] = {
		.keyCode = 'D',
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MoveUp] = {
		.keyCode = 'E',
		.context = InputContextFilter::Common
	};
	m_actionMap[ActionKey::MoveDown] = {
		.keyCode = 'Q',
		.context = InputContextFilter::Common
	};

	// --- Editor Only ---
	m_actionMap[ActionKey::ToggleWireFrame] = {
		.keyCode = VK_F2,
		.context = InputContextFilter::Editor
	};
	m_actionMap[ActionKey::ToggleDebugNormal] = {
		.keyCode = VK_F3,
		.context = InputContextFilter::Editor
	};
	m_actionMap[ActionKey::ToggleGizmoTranslation] = {
		.keyCode = 'W',
		.context = InputContextFilter::Editor
	};
	m_actionMap[ActionKey::ToggleGizmoRotation] = {
		.keyCode = 'E',
		.context = InputContextFilter::Editor
	};
	m_actionMap[ActionKey::ToggleGizmoScaling] = {
		.keyCode = 'R',
		.context = InputContextFilter::Editor
	};
	m_actionMap[ActionKey::Eject] = {
		.keyCode = VK_F8,
		.context = InputContextFilter::Game
	};
	
}

void InputState::Update()
{
	if (!m_bMouseInitialized) return;

	MousePos delta = m_curPos - m_prevPos;
	m_prevPos = m_curPos;

	if (m_bMouseBlocked) m_deltaPos = { 0, 0 };
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

	if (!m_bMouseInitialized)
	{
		m_prevPos = MousePos(x, y);
		m_bMouseInitialized = true;
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
	if (isMouseKey && m_bMouseBlocked) return ActionKeyState::NONE;
	if (!isMouseKey && m_bKeyboardBlocked) return ActionKeyState::NONE;

	auto it = m_actionMap.find(action);
	if (it == m_actionMap.end()) return ActionKeyState::NONE;

	const InputActionData& data = it->second;
	if (m_bGameInputActive && data.context == InputContextFilter::Editor) return ActionKeyState::NONE;
	if (!m_bGameInputActive && data.context == InputContextFilter::Game) return ActionKeyState::NONE;

	auto stateIt = m_keyState.find(data.keyCode);
	return (stateIt != m_keyState.end()) ? stateIt->second : ActionKeyState::NONE;
}

// TODO : 키 매핑 커스터마이징 기능 추가 (특수 키를 위한 문자열 <-> VK 매핑 테이블 추가)
void InputState::LoadKeyBindingsFromIni(const std::wstring& filename)
{
	const wchar_t* section = L"KeyBindings";
	for (const auto& pair : m_actionMap)
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
			m_actionMap[action].keyCode = static_cast<WPARAM>(toupper(value[0]));
		}
	}

}

void InputState::SaveKeyBindingsToIni(const std::wstring& filename)
{
	const wchar_t* section = L"KeyBindings";
	for (const auto& iter : m_actionMap)
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

		wchar_t value[2] = { wchar_t(iter.second.keyCode), 0 };
		WritePrivateProfileStringW(section, keyName, value, filename.c_str());
	}

}

void InputState::BindKey(ActionKey target, WPARAM input, InputContextFilter filter)
{
	m_actionMap[target] = {
		.keyCode = input,
		.context = filter
	};
	m_keyState[input] = ActionKeyState::NONE;
}
