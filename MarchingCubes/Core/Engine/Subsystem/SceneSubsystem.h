#pragma once
#include "Core/Engine/Subsystem/SubSystem.h"
class ISceneSubsystem : public ISubSystem
{
public:
	virtual ~ISceneSubsystem() = default;
	
	virtual void Initialize() = 0;
	virtual void ShutDown() {};
	virtual void Update(float deltaTime) {}

};

