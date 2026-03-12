#pragma once
#include "Core/Engine/Reflection.h"
#include "Core/Engine/Serializer/Serializer.h"
#include "Core/Utils/EnumBitmask.h"
#include <cstdint>

namespace UUIDGenerator
{
	uint64_t Generate();
}

enum class EObjectFlags : uint32_t {
	None = 0,
	Transient = 1 << 0,  // 직렬화 제외
	EditorOnly = 1 << 1, // 에디터에서만 존재하고 빌드 시 제외
    Invisible = 1 << 2,  // 에디터 HierarchyPanel에 노출X
	PendingKill = 1 << 3, // 삭제 대기 중
};
ENABLE_BITMASK(EObjectFlags);

class Entity : public std::enable_shared_from_this<Entity>
{
	REFLECT_GENERATED_BODY(Entity)
public:
	virtual void Serialize(Serializer& ar);

	uint64_t GetUUID() const { return m_uuid; }
	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }
	template <typename T>
    std::shared_ptr<T> GetSharedPtr() { return std::static_pointer_cast<T>(shared_from_this()); }
    template <typename T>
    std::weak_ptr<T> GetWeakPtr() { return std::weak_ptr<T>(GetSharedPtr<T>()); }

    // Flags
    void SetFlags(EObjectFlags flags) { m_objectFlags = flags; }
    void AddFlags(EObjectFlags flags) { m_objectFlags |= flags; }
    void RemoveFlags(EObjectFlags flags) { m_objectFlags &= ~flags; }
    bool HasAnyFlags(EObjectFlags flags) const { return HasFlag(m_objectFlags,flags); }
    bool HasAllFlags(EObjectFlags flags) const { return EqualsFlag(m_objectFlags, flags); }
    bool IsTransient() const { return HasAnyFlags(EObjectFlags::Transient); }

protected:
	uint64_t m_uuid = UUIDGenerator::Generate();
	std::string m_name = "";
    EObjectFlags m_objectFlags = EObjectFlags::None;
};

