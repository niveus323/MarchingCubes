#include "pch.h"
#include "GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(GameObject, Entity)
    REFLECT_PROPERTY(m_bActive, EPropertyType::Bool, "Active")
END_REFLECTION()

void GameObject::Init()
{
    m_objectID = GetScene()->AllocateObjectID(this);
}

void GameObject::Destroy()
{
    for (auto& comp : m_components) 
        comp->Destroy();
    for (auto& child : m_children) 
        child->Destroy();

    m_components.clear();
    m_children.clear();
}

void GameObject::Serialize(Serializer& ar)
{
	Entity::Serialize(ar);
    size_t compCount = m_components.size();
    ar.BeginArray("Components", compCount);
	if (ar.IsSaving())
	{
        for (auto& comp : m_components)
        {
            if (comp->IsTransient()) continue;

            ar.BeginObject("Component");
            std::string className = comp->GetType()->GetName();
            ar.Serialize("Type", className);
            comp->Serialize(ar);
            ar.EndObject();
        }
	}
    else
    {
        for (int i = 0; i < compCount; ++i)
        {
            ar.BeginObject("Component");

            std::string className;
            ar.Serialize("Type", className);
            std::string compName;
            ar.Serialize("Name", compName);

            Component* targetComp = nullptr;
            if (TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(className))
            {
                for (auto& existing : m_components) 
                {
                    if (existing->GetType() == typeDesc)
                    {
                        if (!compName.empty() && existing->GetName() != compName) continue;

                        targetComp = existing.get();
                        break;
                    }
                }
                
                if (!targetComp)
                {
                    if (Component* newObj = static_cast<Component*>(typeDesc->CreateInstance()))
                    {
                        std::shared_ptr<Component> newComp(newObj);
                        newComp->SetOwner(GetSharedPtr<GameObject>());
                        newComp->Init(); // NOTE : Init 호출 타이밍의 안전성 검사 필요.
                        targetComp = newComp.get();
                        m_components.push_back(std::move(newComp));
                    }
                    else
                    {
                        Log::Print(ELogVerbosity::Fatal, "Serialize", "Invalid Component Type : {}", className);
                    }
                }
            }
            else
            {
                Log::Print(ELogVerbosity::Fatal, "Serialize", "Unknown Component Type: {}", className);
            }

            if (targetComp)
            {
                targetComp->Serialize(ar);
            }

            ar.EndObject();
        }
    }
    ar.EndArray();

    // Child
    size_t childCount = m_children.size();
    ar.BeginArray("Childs", childCount);
    if (ar.IsSaving())
    {
        for (auto& child : m_children)
        {
            if (child->IsTransient()) continue;

            ar.BeginObject("Child");
            std::string className = child->GetType()->GetName();
            ar.Serialize("Type", className);
            child->Serialize(ar);
            ar.EndObject();
        }
    }
    else
    {
        for (int i = 0; i < childCount; ++i)
        {
            ar.BeginObject("Child");

            std::string className;
            ar.Serialize("Type", className);

            if (TypeDescriptor* typeDesc = ReflectionRegistry::Get().GetType(className))
            {
                if (GameObject* newObj = static_cast<GameObject*>(typeDesc->CreateInstance()))
                {
                    std::shared_ptr<GameObject> newChild(newObj);   
                    newChild->Serialize(ar);
                    GameObject* ptr = newChild.get();
                    AddChild(std::move(newChild));
                    ptr->Init();
                }
                else
                {
                    Log::Print(ELogVerbosity::Fatal, "Serialize", "Invalid Child Object Type : Object Type is {}", className);
                }
            }
            else
            {
                Log::Print(ELogVerbosity::Fatal, "Serialize", "Unknown Object Type: {}", className);
            }

            ar.EndObject();
        }
    }
    ar.EndArray();
}

void GameObject::MarkForDestroy()
{
    AddFlags(EObjectFlags::PendingKill);
    for (auto& child : m_children)
    {
        child->MarkForDestroy();
    }
}

Component* GameObject::AddComponent(TypeDescriptor* typeDesc, EObjectFlags flags)
{
    if (!typeDesc) return nullptr;

    // Dependency Injection (Reflection에 등록된 종속 컴포넌트들을 체크하여 함께 생성 및 주입)
    for (TypeDescriptor* reqType : typeDesc->GetRequiredComponents())
    {
        bool bHasRequired = false;
        for (auto& existing : m_components)
        {
            if (existing->GetType() == reqType || existing->GetType()->IsDerivedFrom(reqType->GetName()))
            {
                bHasRequired = true;
                break;
            }
        }

        // 없다면 재귀적으로 먼저 추가
        if (!bHasRequired) AddComponent(reqType, flags);
    }

    if (Component* newComp = static_cast<Component*>(typeDesc->CreateInstance()))
    {
        return InternalAddComponent(std::shared_ptr<Component>(newComp), flags);
    }
    return nullptr;
}

void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    if (!child || child.get() == this) return;

    if (auto oldParent = child->GetOwner())
    {
        oldParent->RemoveChild(child);
    }
    child->m_owner = this->GetSharedPtr<GameObject>();
    child->SetScene(m_scene.lock()->GetSharedPtr<Scene>());

    if (auto scenePtr = m_scene.lock())
    {
        if (!scenePtr->FindEntity(child->GetUUID()))
        {
            scenePtr->AddObject(child);
        }
    }
    m_children.push_back(std::move(child));
}

