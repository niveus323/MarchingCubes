#pragma once
#include "Core/Scene/Object/GameObject.h"

class Pawn;
class IUIBuilder;

class Controller : public GameObject
{
	REFLECT_GENERATED_BODY(Controller)
public:
	virtual void Update(float deltaTime) override;
	virtual void Possess(Pawn* pawn);
	virtual void UnPossess();
	virtual void RenderUI(IUIBuilder* ui) {};
	
	Pawn* GetPawn() { return m_possessed; }
protected:
	virtual void ProcessInput(float deltaTime) = 0;

protected:
	Pawn* m_possessed = nullptr;

};

