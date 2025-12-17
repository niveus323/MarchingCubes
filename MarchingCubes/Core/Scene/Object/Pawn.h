#pragma once
#include "SceneObject.h"

class Pawn : public SceneObject
{
public:
	Pawn(Scene* scene) : SceneObject(scene) {}
	virtual ~Pawn() = default;

	virtual void OnPossess() {}
	virtual void OnUnPossess() {}

	virtual void AddMovementInput(DirectX::XMVECTOR dir, float scale);
	virtual void AddControllerYawInput(float val);
	virtual void AddControllerPitchInput(float val);

protected:
	
private:

};

