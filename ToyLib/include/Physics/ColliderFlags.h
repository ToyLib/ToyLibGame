#pragma once
#include <cstdint>

namespace toy {

//------------------------------------------------------------------------------
// ビットマスク enum 用ヘルパー
//------------------------------------------------------------------------------
#define ENABLE_BITMASK_OPERATORS(x)                                      \
inline x operator|(x a, x b) {                                           \
    return static_cast<x>(                                               \
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));            \
}                                                                        \
inline x operator&(x a, x b) {                                           \
    return static_cast<x>(                                               \
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));            \
}                                                                        \
inline x& operator|=(x& a, x b) { a = a | b; return a; }                 \
inline x& operator&=(x& a, x b) { a = a & b; return a; }                 \
inline x operator~(x a) {                                                \
    return static_cast<x>(~static_cast<uint32_t>(a));                   \
}

//------------------------------------------------------------------------------
// ColliderFlags
//------------------------------------------------------------------------------
enum ColliderFlags : uint32_t
{
    C_NONE = 0,

    // Team / Faction
    C_PLAYER_TEAM = 1u << 0,
    C_ENEMY_TEAM  = 1u << 1,

    // Combat roles
    C_HURTBOX     = 1u << 2,
    C_HITBOX      = 1u << 3,

    // World
    C_WALL        = 1u << 4,
    C_GROUND      = 1u << 5,
    C_FOOT        = 1u << 6,
    C_CEILING     = 1u << 7,

    // Sensor
    C_SENSOR      = 1u << 8,

    // Physics body（★追加）
    C_BODY        = 1u << 9
};

ENABLE_BITMASK_OPERATORS(ColliderFlags)

} // namespace toy
