#pragma once

namespace toy { class ColliderComponent; }

namespace toy::kit {

//=============================================================================
// CollisionEvent
//  Prefab のコライダーが何かと接触している「事実」を通知する（設計方針 7）。
//  接触が何を意味するか（攻撃か/障害物か等）は Game Logic が
//  other->GetFlags() を見て判断する。
//=============================================================================
struct CollisionEvent
{
    toy::ColliderComponent* other = nullptr;
};

//=============================================================================
// GroundedEvent
//  接地状態が変化した「事実」を通知する（IsGrounded の立ち上がり/立ち下がり）。
//=============================================================================
struct GroundedEvent
{
    bool grounded = false;
};

} // namespace toy::kit
