#pragma once
#include "Core/Engine/Subsystem/SubSystem.h"
#include "Core/Engine/Reflection.h"

class ISceneSubsystem : public ISubSystem
{
	REFLECT_GENERATED_BODY(ISceneSubsystem)
public:
	virtual ~ISceneSubsystem() = default;
	
	virtual void Initialize() = 0;
	virtual void ShutDown() {};
	virtual void Update(float deltaTime) {}

};

