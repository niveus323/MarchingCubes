#pragma once
#include <unordered_map>

enum class ActionKey
{
	Escape,
	Ctrl,
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

class InputState
{
public:
	InputState();
	~InputState() = default;

	void OnMouseDown(int x, int y, WPARAM btn);
	void OnMouseUp(int x, int y, WPARAM btn);
	void OnMouseMove(int x, int y);
	void OnKeyDown(WPARAM key);
	void OnKeyUp(WPARAM key);
	void Update();

	bool IsPressed(ActionKey action) const;
	ActionKeyState GetKeyState(ActionKey action) const;

	//Ini
	void LoadKeyBindingsFromIni(const std::wstring& filename);
	void SaveKeyBindingsToIni(const std::wstring& filename);

public:
	int m_mouseX;
	int m_mouseY;
	int m_prevMouseX;
	int m_prevMouseY;
	float m_mouseDeltaX;
	float m_mouseDeltaY;

	bool m_mouseInitialized;
	ActionKeyState m_leftBtnState;
	ActionKeyState m_rightBtnState;

	std::unordered_map<ActionKey, WPARAM> m_keyMap;
	std::unordered_map<WPARAM, ActionKeyState> m_keyState;


};

