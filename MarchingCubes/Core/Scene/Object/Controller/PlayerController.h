#pragma once
#include "Controller.h"
#include "Core/Input/InputMappingContext.h"
#include <vector>

class PlayerController : public Controller
{
    REFLECT_GENERATED_BODY(PlayerController)
public:
    void AddMappingContext(std::shared_ptr<InputMappingContext> context) { m_activeContexts.push_back(context); }
    void RemoveMappingContext(std::shared_ptr<InputMappingContext> context) { std::erase(m_activeContexts, context); }
    void ClearMappingContexts() { m_activeContexts.clear(); }

protected:
    virtual void ProcessInput(float deltaTime) override;

private:
    std::vector<std::shared_ptr<InputMappingContext>> m_activeContexts;

    bool m_bShowMouseCursor = false;
};

