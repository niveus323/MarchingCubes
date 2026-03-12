#include "pch.h"
#include "PlayerController.h"
#include "Core/Engine/EngineCore.h"

BEGIN_REFLECTION(PlayerController, Controller)
END_REFLECTION()

void PlayerController::ProcessInput(float deltaTime)
{
    if (!m_bInputEnabled) return;

    auto input = EngineCore::GetInputState();
    if (!input) return;

    for (const auto& context : m_activeContexts)
    {
        for (const auto& action : context->m_actions)
        {
            if (input->GetKeyState(action.key) == action.triggerState)
            {
                if (action.handler) action.handler();
            }
        }

        for (const auto& axis : context->m_axes)
        {
            float value = input->GetAxisValue(axis.key);
            float finalValue = value * axis.scale;

            if (finalValue != 0.0f && axis.handler)
            {
                axis.handler(finalValue);
            }
        }
    }
}