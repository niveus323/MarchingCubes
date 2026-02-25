#include "pch.h"
#include "Entity.h"
#include <random>
#include <memory>

BEGIN_REFLECTION_ROOT(Entity)
END_REFLECTION()

namespace UUIDGenerator
{
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_Engine(s_RandomDevice());
    static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

    uint64_t Generate()
    {
        uint64_t uuid = 0;
        while (uuid == 0) uuid = s_UniformDistribution(s_Engine);
        return uuid;
    }
}

void Entity::Serialize(Serializer& ar)
{
    ar.Serialize("UUID", m_uuid);
    ar.Serialize("Name", m_name);
    
    if (ar.IsSaving())
    {
        ar.RegisterEntity(m_uuid, this);
    }
}
