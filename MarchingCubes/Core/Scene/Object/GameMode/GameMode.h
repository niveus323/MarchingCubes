#pragma once
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Object/Controller/Controller.h"

// Forward Declaration
class Pawn;
class Scene;

class GameMode : public GameObject
{
	REFLECT_GENERATED_BODY(GameMode)
public:	
	virtual void Init() override;
	virtual void Destroy() override;
	virtual void BeginPlay() override;
	virtual void EndPlay() override;

	template<std::derived_from<Controller> T>
	T* GetController() { return static_cast<T*>(m_pc); }

private:
	Controller* m_pc = nullptr;
};

