#pragma once
#include "GameObject.h"

// Forward Declaration
class PlayerController;
class Pawn;
class Scene;

class GameMode : public GameObject
{
public:
	GameMode(Scene* scene) : GameObject(scene) {}
	virtual ~GameMode()
	{
		m_pc = nullptr;
	}
	
	virtual void Init() override;
	
	PlayerController* GetPlayerController() { return m_pc; }

protected:

private:
	PlayerController* m_pc = nullptr;

};

