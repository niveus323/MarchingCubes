#pragma once
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Object/Controller/Controller.h"

// Forward Declaration
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
	
	template<std::derived_from<Controller> T>
	T* GetController() { return static_cast<T*>(m_pc); }

private:
	Controller* m_pc = nullptr;
};

