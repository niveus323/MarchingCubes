#pragma once
#include <unordered_map>

enum class ActionKey
{
	Escape,
	Ctrl,
	Shift,
	MouseX,
	MouseY,
	LeftClick, 
	RightClick, 
	MiddleClick,
	MoveForward,
	MoveBackward,
	MoveLeft,
	MoveRight,
	MoveUp,
	MoveDown,

	// --- Editor Only ---
	ToggleDebugView,
	ToggleWireFrame,
	ToggleDebugNormal,
	ToggleGizmoTranslation,
	ToggleGizmoRotation,
	ToggleGizmoScaling,
	Eject,
	Count
};

enum class ActionKeyState
{
	NONE, 
	JustReleased,
	JustPressed,
	Pressed,
};

enum class InputContextFilter
{
	Common,
	Game,
	Editor
};

struct InputActionData
{
	WPARAM keyCode;
	InputContextFilter context;
};

struct MousePos
{
	int x;
	int y;

	MousePos(int _x = 0, int _y = 0) : x(_x), y(_y) {}
	MousePos(LPARAM lParam)
	{
		x = GET_X_LPARAM(lParam);
		y = GET_Y_LPARAM(lParam);
	}

	bool operator==(const MousePos& other) const { return x == other.x && y == other.y; }
	bool operator!=(const MousePos& other) const { return !(*this == other); }
	MousePos& operator+=(const MousePos& other) { x += other.x; y += other.y; return *this; }
	MousePos& operator-=(const MousePos& other) { x -= other.x; y -= other.y; return *this; }
	MousePos operator-() const { return MousePos(-x, -y); }
	MousePos operator+(const MousePos& other) const { return MousePos( x + other.x, y + other.y ); }
	MousePos operator-(const MousePos& other) const { return MousePos( x - other.x, y - other.y ); }
};

class InputState
{
public:
	InputState();
	~InputState() = default;

	void Update();
	void OnMouseMove(int x, int y);
	void OnMouseDown(int x, int y, WPARAM btn) 
	{ 
		m_keyState[btn] = ActionKeyState::JustPressed; 
		SetMousePos(x, y); 
	}
	void OnMouseUp(int x, int y, WPARAM btn) { m_keyState[btn] = ActionKeyState::JustReleased; SetMousePos(x, y); }
	void OnKeyDown(WPARAM key) { 
		m_keyState[key] = ActionKeyState::JustPressed; 
	}
	void OnKeyUp(WPARAM key) { m_keyState[key] = ActionKeyState::JustReleased; }
	bool IsPressed(ActionKey action) const;
	bool IsPressedOnce(ActionKey action) const { return GetKeyState(action) == ActionKeyState::JustPressed; }
	float GetAxisValue(ActionKey action) const;
	ActionKeyState GetKeyState(ActionKey action) const;
	MousePos GetMousePos() const { return m_curPos; }
	void SetInputBlocked(bool bBlockMouse, bool bBlockKeyboard)
	{
		m_bMouseBlocked = bBlockMouse;
		m_bKeyboardBlocked = bBlockKeyboard;
	}

	//Ini
	void LoadKeyBindingsFromIni(const std::wstring& filename);
	void SaveKeyBindingsToIni(const std::wstring& filename);

	void SetGameInputActive(bool bActive) { m_bGameInputActive = bActive; }
	bool IsGameInputActive() const { return m_bGameInputActive; }

	void BindKey(ActionKey target, WPARAM input, InputContextFilter filter = InputContextFilter::Common);
private:
	void SetMousePos(int x, int y) { m_curPos = MousePos(x,y); }
	void SetMousePos(MousePos pos) { m_curPos = pos; }

private:
	bool m_bMouseBlocked = false;
	bool m_bKeyboardBlocked = false;
	bool m_bMouseInitialized = false;
	bool m_bGameInputActive = false;

	MousePos m_curPos{};
	MousePos m_prevPos{};
	MousePos m_deltaPos{};

	std::unordered_map<WPARAM, ActionKeyState> m_keyState;
	std::unordered_map<ActionKey, InputActionData> m_actionMap;
};

