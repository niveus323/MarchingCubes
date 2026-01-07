#pragma once
#include <unordered_map>

enum class ActionKey
{
	Escape,
	Ctrl,
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
	ToggleDebugView,
	ToggleWireFrame,
	ToggleDebugNormal,
	Count
};

enum class ActionKeyState
{
	NONE, 
	JustReleased,
	JustPressed,
	Pressed,
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
	void OnMouseDown(int x, int y, WPARAM btn) { m_keyState[btn] = ActionKeyState::JustPressed; SetMousePos(x, y); }
	void OnMouseUp(int x, int y, WPARAM btn) { m_keyState[btn] = ActionKeyState::JustReleased; SetMousePos(x, y); }
	void OnKeyDown(WPARAM key) { m_keyState[key] = ActionKeyState::JustPressed; }
	void OnKeyUp(WPARAM key) { m_keyState[key] = ActionKeyState::JustReleased; }
	bool IsPressed(ActionKey action) const;
	float GetAxisValue(ActionKey action) const;
	ActionKeyState GetKeyState(ActionKey action) const;
	MousePos GetMousePos() const { return m_curPos; }
	void SetInputCaptured(bool capturedMouse, bool capturedKeyboard)
	{
		m_isMouseCaptured = capturedMouse;
		m_isKeyboardCaptured = capturedKeyboard;
	}

	//Ini
	void LoadKeyBindingsFromIni(const std::wstring& filename);
	void SaveKeyBindingsToIni(const std::wstring& filename);

private:
	void SetMousePos(int x, int y) { m_curPos = MousePos(x,y); }
	void SetMousePos(MousePos pos) { m_curPos = pos; }

private:
	bool m_isMouseCaptured = false;
	bool m_isKeyboardCaptured = false;

	MousePos m_curPos{};
	MousePos m_prevPos{};
	MousePos m_deltaPos{};

	bool m_mouseInitialized = false;

	std::unordered_map<ActionKey, WPARAM> m_keyMap;
	std::unordered_map<WPARAM, ActionKeyState> m_keyState;

};

