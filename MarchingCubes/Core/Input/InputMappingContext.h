#pragma once
#include "Core/Input/InputState.h"
#include <functional>
#include <vector>
using InputActionHandler = std::function<void()>;
using InputAxisHandler = std::function<void(float)>;

struct ActionBinding
{
    ActionKey key;
    ActionKeyState triggerState;
    InputActionHandler handler;
};

struct AxisBinding
{
    ActionKey key;
    float scale;
    InputAxisHandler handler;
};

class InputMappingContext
{
public:
    void BindAction(ActionKey key, ActionKeyState state, std::function<void()> func)
    {
        m_actions.push_back({ key, state, func });
    }

    void BindAxis(ActionKey key, float scale, std::function<void(float)> func)
    {
        m_axes.push_back({ key, scale, func });
    }

public:
    std::vector<ActionBinding> m_actions;
    std::vector<AxisBinding> m_axes;
};

