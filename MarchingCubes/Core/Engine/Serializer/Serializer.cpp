#include "pch.h"
#include "Serializer.h"

void Serializer::RegisterEntity(uint64_t uuid, Entity* ptr)
{
    if (!IsSaving()) m_entityMap[uuid] = ptr;
}

void Serializer::ResolvePointers()
{
    for (auto& fixup : m_ptrFixups)
    {
        auto it = m_entityMap.find(fixup.uuid);
        if (it != m_entityMap.end())
        {
            fixup.binder(it->second);
        }
    }
    m_entityMap.clear();
}
