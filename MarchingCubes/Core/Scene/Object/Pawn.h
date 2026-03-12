#pragma once
#include "SceneObject.h"

// 로컬 플레이어를 위한 
enum class EAutoPossessTarget : int8_t
{
	Disabled = -1,
	Player0 = 0,
	Player1 = 1,
	Player2 = 2,
	Player3 = 3
};

class Pawn : public SceneObject
{
	REFLECT_GENERATED_BODY(Pawn)
public:
	virtual void BeginPlay() override;
	virtual void OnPossess();
	virtual void OnUnPossess() {}

	virtual void AddMovementInput(DirectX::XMVECTOR dir, float scale);
	virtual void AddControllerYawInput(float val);
	virtual void AddControllerPitchInput(float val);

private:
	EAutoPossessTarget m_autoPossess = EAutoPossessTarget::Disabled;
};

