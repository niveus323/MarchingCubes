#pragma once
#include "Core/Scene/Object/GameObject.h"

//Forward Declaration
class Pawn;
class CameraComponent;
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
	bool IsInputEnabled() const { return m_bInputEnabled; }
	void SetInputEnabled(bool bEnabled) { m_bInputEnabled = bEnabled; }
	CameraComponent* GetPossessdCamera();
protected:
	virtual void ProcessInput(float deltaTime) = 0;
	CameraComponent* m_targetCam = nullptr;
	Pawn* m_possessed = nullptr;
	bool m_bInputEnabled = false;
};

