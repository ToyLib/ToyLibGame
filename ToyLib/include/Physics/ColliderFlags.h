#pragma once
#include <cstdint>


namespace toy {

//------------------------------------------------------------------------------
// ビットマスク enum 用ヘルパーマクロ
//------------------------------------------------------------------------------
// enum class ではなく通常の enum を bitflag として扱うための演算子群。
// ColliderType のように「複数種類の属性を OR で持つ」用途で使用する。
// 例） C_PLAYER | C_FOOT など
//------------------------------------------------------------------------------
#define ENABLE_BITMASK_OPERATORS(x)                                \
inline x operator|(x a, x b) { return static_cast<x>(static_cast<int>(a) | static_cast<int>(b)); } \
inline x operator&(x a, x b) { return static_cast<x>(static_cast<int>(a) & static_cast<int>(b)); } \
inline x& operator|=(x& a, x b) { a = a | b; return a; }            \
inline x& operator&=(x& a, x b) { a = a & b; return a; }            \
inline x operator~(x a) { return static_cast<x>(~static_cast<int>(a)); }

//------------------------------------------------------------------------------
// ColliderType
//------------------------------------------------------------------------------
// 衝突カテゴリをビットフラグで表現する。
// 1つの Collider が複数フラグを同時に持つことも可能。
// 例）プレイヤーの足用コライダー: C_PLAYER | C_FOOT
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
    
    // センサー
    C_SENSOR      = 1u << 8
};
ENABLE_BITMASK_OPERATORS(ColliderFlags)

} // namespace toy
