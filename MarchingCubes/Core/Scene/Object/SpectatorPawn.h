#pragma once
#include "Core/Scene/Object/Pawn.h"

// Forward Declaration
class CameraComponent;

class SpectatorPawn : public Pawn
{
public:
    SpectatorPawn(Scene* scene);

    virtual void Init() override;

private:
    CameraComponent* m_cameraComp = nullptr;
};

