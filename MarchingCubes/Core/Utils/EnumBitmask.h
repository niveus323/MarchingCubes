#pragma once
#include <type_traits>

template <typename Enum>
inline constexpr bool enable_bitmask_v = false;

#define ENABLE_BITMASK(EnumName) \
    template <> \
    inline constexpr bool enable_bitmask_v<EnumName> = true;

template <typename Enum>
concept BitmaskEnum = std::is_enum_v<Enum> && enable_bitmask_v<Enum>;

// 비트 연산자 오버로딩
template <BitmaskEnum Enum>
constexpr Enum operator|(Enum lhs, Enum rhs) {
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <BitmaskEnum Enum>
constexpr Enum operator&(Enum lhs, Enum rhs) {
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <BitmaskEnum Enum>
constexpr Enum operator^(Enum lhs, Enum rhs) {
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <BitmaskEnum Enum>
constexpr Enum operator~(Enum rhs) {
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(~static_cast<underlying>(rhs));
}

// 복합 대입 연산자
template <BitmaskEnum Enum>
constexpr Enum& operator|=(Enum& lhs, Enum rhs) {
    lhs = lhs | rhs;
    return lhs;
}

template <BitmaskEnum Enum>
constexpr Enum& operator&=(Enum& lhs, Enum rhs) {
    lhs = lhs & rhs;
    return lhs;
}

template <BitmaskEnum Enum>
constexpr Enum& operator^=(Enum& lhs, Enum rhs) {
    lhs = lhs ^ rhs;
    return lhs;
}

// 편의 함수
template <BitmaskEnum Enum>
constexpr bool EqualsFlag(Enum value, Enum flag) {
    return (value & flag) == flag;
}

template <BitmaskEnum Enum>
constexpr bool HasFlag(Enum value, Enum flag) {
    return (value & flag) != static_cast<Enum>(0);
}