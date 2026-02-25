#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <type_traits>

// Forward Declaration
class Entity;

// 벡터 타입 대응을 위한 std::concept
template <typename T>
concept VectorLike = requires(T a) {
    a.x;
    a.y;
};
// 지연 연결을 위한 구조체
struct PtrFixupInfo
{
    uint64_t uuid;
    std::function<void(Entity*)> binder;
};

class Serializer
{
public:
	Serializer(bool bSaving) : m_bSaving(bSaving){ }
    virtual ~Serializer() = default;

    virtual void Serialize(const std::string& name, int& val) = 0;
    virtual void Serialize(const std::string& name, float& val) = 0;
    virtual void Serialize(const std::string& name, std::string& val) = 0;
    virtual void Serialize(const std::string& name, bool& val) = 0;
    virtual void Serialize(const std::string& name, uint32_t& val) = 0;
    virtual void Serialize(const std::string& name, uint64_t& val) = 0;
    void Serialize(const std::string& name, VectorLike auto& val)
    {
        BeginObject(name);
        Serialize("x", val.x);
        Serialize("y", val.y);
        if constexpr (requires { val.z; }) Serialize("z", val.z);
        if constexpr (requires { val.w; }) Serialize("w", val.w);
        EndObject();
    }

    template <typename T>
    void Serialize(const std::string& name, T& obj)
        requires (!VectorLike<T> && !std::is_fundamental_v<T>) // 벡터형 혹은 기본형이 아닌 타입 직렬화
    {
        BeginObject(name);
        obj.Serialize(*this);
        EndObject();
    }

    template <typename T>
    void Serialize(const std::string& name, std::vector<T>& vec) //std::vector 등 컨테이너 직렬화
    {
        size_t count = vec.size();
        BeginArray(name, count);

        if (!IsSaving()) 
        {
            vec.resize(count); // 로드 시 벡터 크기 확보
        }

        for (auto& element : vec) 
        {
            Serialize("element", element);
        }
        EndArray();
    }

    template<typename T>
    void SerializePtr(const std::string& name, T*& ptr)
    {
        static_assert(std::is_base_of<Entity, T>::value, "T must derive from Entity");

        if (IsSaving())
        {
            uint64_t uuid = (ptr) ? ptr->GetUUID() : 0;
            Serialize(name, uuid);
        }
        else
        {
            uint64_t uuid = 0;
            Serialize(name, uuid);

            if (uuid != 0)
            {
                m_ptrFixups.push_back({ uuid,
                    [&ptr](Entity* foundEntity) {
                        ptr = dynamic_cast<T*>(foundEntity);
                    }
                });
                ptr = nullptr;
            }
            else
            {
                ptr = nullptr;
            }
        }
    }
    void RegisterEntity(uint64_t uuid, Entity* ptr);
    void ResolvePointers();

    // JSON Hierarachy
    virtual void BeginObject(const std::string& name) {}
    virtual void EndObject() {}
    
    // JSON Array
    virtual void BeginArray(const std::string& name, size_t& count) = 0;
    virtual void EndArray() {}
    
    bool IsSaving() { return m_bSaving; }

private:
	bool m_bSaving = true;
    std::unordered_map<uint64_t, Entity*> m_entityMap;
    std::vector<PtrFixupInfo> m_ptrFixups;
};

#define SERIALIZE_PROPERTY(ar, var) ar.Serialize(#var, var)

