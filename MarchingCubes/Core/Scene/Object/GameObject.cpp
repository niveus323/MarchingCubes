#include "pch.h"
#include "GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(GameObject, Entity)
    REFLECT_PROPERTY(m_bActive, EPropertyType::Bool)
END_REFLECTION()

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
                        Log::Print("Invalid Component Type : %s", className.c_str());
                    }
                }
            }
            else
            {
                Log::Print("Unknown Component Type: %s", className.c_str());
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
                    Log::Print("Invalid Child Object Type : Object Type is %s", className.c_str());
                }
            }
            else
            {
                Log::Print("Unknown Object Type: %s", className.c_str());
            }

            ar.EndObject();
        }
    }
    ar.EndArray();
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
    m_children.push_back(std::move(child));
}

