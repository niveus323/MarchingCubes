#pragma once
#include "GameObject.h"

class Pawn;

class PlayerController : public GameObject
{
public:
	PlayerController(Scene* scene) : GameObject(scene) {}
	virtual ~PlayerController() = default;

	virtual void Update(float deltaTime) override;
	virtual void Possess(Pawn* pawn);
	virtual void UnPossess();

	Pawn* GetPawn() { return m_possessed; }
	float GetMoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float speed) { m_moveSpeed = speed; }

protected:
	virtual void ProcessInput(float deltaTime);

private:
	Pawn* m_possessed = nullptr;
	float m_controlYaw = 0.0f;
	float m_controlPitch = 0.0f;
	float m_mouseSensitivity = 0.01f;
	float m_moveSpeed = 100.0f;
};

