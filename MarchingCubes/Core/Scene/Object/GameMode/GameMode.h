#pragma once
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Component/TransformComponent.h"

// Forward Declaration
class Scene;
class Pawn;
class PlayerController;

class GameMode : public GameObject
{
	REFLECT_GENERATED_BODY(GameMode)
public:	
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void Update(float deltaTime) override;
	virtual void Serialize(Serializer& ar) override;

	PlayerController* GetController(int playerIndex = 0);

private:
	std::map<int, PlayerController*> m_playerControllers;

	bool m_bBeginPlayActivated = false;
	std::string m_controllerClass = "PlayerController"; // 게임 실행 시 스폰할 Controller 클래스
	std::string m_defaultPawnClass = "SpectatorPawn"; // 게임 실행 시 Possess 설정을 한 Pawn이 없을 경우 스폰할 Pawn 클래스
	Transform m_defualtPawnInitialTransform{};

	// TODO : GameState, HUD 구현 시 아래 주석 해제
	//std::string m_gameStateClass = "";
	//std::string m_HUDClass = "";
};