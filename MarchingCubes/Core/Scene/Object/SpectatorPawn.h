#pragma once
#include "Core/Scene/Object/Pawn.h"

// Forward Declaration
class CameraComponent;

class SpectatorPawn : public Pawn
{
    REFLECT_GENERATED_BODY(SpectatorPawn)
public:
    virtual void Init() override;

private:
    CameraComponent* m_cameraComp = nullptr;
};

