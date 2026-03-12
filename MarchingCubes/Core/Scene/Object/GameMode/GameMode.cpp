#include "pch.h"
#include "GameMode.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/Controller/PlayerController.h"
#include "Core/Scene/Object/Pawn.h"

BEGIN_REFLECTION(GameMode, GameObject)
    REFLECT_PROPERTY_CLASS(m_controllerClass, PlayerController, "ControllerClass")
    REFLECT_PROPERTY_CLASS(m_defaultPawnClass, Pawn, "DefaultPawnClass")
    REFLECT_PROPERTY(m_defualtPawnInitialTransform.position, EPropertyType::Vector3, "Spawn Position")
    REFLECT_PROPERTY(m_defualtPawnInitialTransform.rotation, EPropertyType::Vector3, "Spawn Rotation")
    REFLECT_PROPERTY(m_defualtPawnInitialTransform.scale, EPropertyType::Vector3, "Spawn Scale")
END_REFLECTION()

void GameMode::BeginPlay()
{
    GameObject::BeginPlay();
    m_bBeginPlayActivated = false;
    if (auto scene = GetScene())
    {
        // 등록된 PlayerController 클래스 문자열을 기반으로 동적 생성
        TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(m_controllerClass);
        PlayerController* newPC = nullptr;
        if (typeDesc) newPC = static_cast<PlayerController*>(typeDesc->CreateInstance());
        if (!newPC) newPC = scene->CreateObject<PlayerController>("PlayerController", EObjectFlags::Transient); // 실패 시 기본 PlayerController 생성

        m_playerControllers[0] = newPC; // Player 0에 할당
    }
}

void GameMode::EndPlay()
{
    GameObject::EndPlay();
    m_playerControllers.clear();
    m_bBeginPlayActivated = false;
}

void GameMode::Update(float deltaTime)
{
    GameObject::Update(deltaTime);

    // Pawn 스폰은 지연 생성(Pawn의 Auto Possess를 체크하기 위해)
    if (!m_bBeginPlayActivated && !m_playerControllers.empty())
    {
        m_bBeginPlayActivated = true;
        auto scene = GetScene();
        assert(scene && "GameMode Error : Scene is Invalid!!!!");

        // Default Pawn 스폰
        for (auto& [index, pc] : m_playerControllers)
        {
            if (pc->GetPawn()) continue;
            if (TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(m_defaultPawnClass))
            {
                if (Pawn* newPawn = static_cast<Pawn*>(typeDesc->CreateInstance()))
                {
                    std::shared_ptr<Pawn> pawnPtr(newPawn);
                    scene->AddObject(pawnPtr);
                    scene->SetName(std::format("Default Pawn_%d", index));
                    newPawn->Init();
                    newPawn->SetTransform(m_defualtPawnInitialTransform);
                    pc->Possess(newPawn);
                }
            }
        }
    }
}

void GameMode::Serialize(Serializer& ar)
{
    GameObject::Serialize(ar);

    // 디폴트 생성 Controller 클래스
    ar.Serialize("DefaultControllerClass", m_controllerClass);
    // 디폴트 생성 Pawn 클래스
    ar.Serialize("DefaultPawnClass", m_defaultPawnClass);
    // 디폴트로 생성되는 Pawn의 트랜스폼
    ar.Serialize("DefaultPawnTransform", m_defualtPawnInitialTransform);
}

PlayerController* GameMode::GetController(int playerIndex)
{
    auto it = m_playerControllers.find(playerIndex);
    if (it != m_playerControllers.end())
        return it->second;

    return nullptr;
}