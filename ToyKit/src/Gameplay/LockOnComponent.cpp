#include "Gameplay/LockOnComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Physics/PhysWorld.h"
#include "Physics/ColliderComponent.h"

#include <limits>

namespace toy::kit {

LockOnComponent::LockOnComponent(toy::Actor* owner, const Desc& desc, int updateOrder)
    : Component(owner, updateOrder)
{
}

void LockOnComponent::SetTarget(toy::Actor* t, bool lock)
{
    mTarget = t;
    mLocked = (lock && mTarget);
}

void LockOnComponent::Toggle()
{
    if (IsLocked())
    {
        Unlock();
        return;
    }

    mTarget = PickNearestTarget();
    mLocked = (mTarget != nullptr);
}

void LockOnComponent::Unlock()
{
    mLocked = false;
    mTarget = nullptr;
}

void LockOnComponent::Update(float /*dt*/)
{
    if (!IsLocked())
    {
        return;
    }

    // 距離で解除（最小）
    const Vector3 toT = mTarget->GetPosition() - GetOwner()->GetPosition();
    if (toT.Length() > mDesc.breakDist)
    {
        Unlock();
    }
}

Vector3 LockOnComponent::FlattenAndNormalize(const Vector3& v) const
{
    Vector3 r = v;
    if (mDesc.keepYawOnly)
    {
        r.y = 0.0f;
    }
    if (r.LengthSq() < Math::NearZeroEpsilon)
    {
        return Vector3::Zero;
    }
    r.Normalize();
    return r;
}

Vector3 LockOnComponent::GetBasisForward() const
{
    if (!GetOwner())
    {
        return Vector3::Zero;
    }

    if (mDesc.mode == BasisMode::Camera)
    {
        // 既存 DirMoveComponent と同じ：invViewMatrix の軸から取る
        const Matrix4& invV = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix();
        Vector3 f = invV.GetZAxis(); // “カメラの前”
        return FlattenAndNormalize(f);
    }
    else
    {
        return FlattenAndNormalize(GetOwner()->GetForward());
    }
}

Vector3 LockOnComponent::GetBasisRight() const
{
    if (!GetOwner())
    {
        return Vector3::Zero;
    }

    if (mDesc.mode == BasisMode::Camera)
    {
        const Matrix4& invV = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix();
        Vector3 r = invV.GetXAxis();
        return FlattenAndNormalize(r);
    }
    else
    {
        return FlattenAndNormalize(GetOwner()->GetRight());
    }
}

toy::Actor* LockOnComponent::PickNearestTarget() const
{
    toy::Actor* owner = GetOwner();
    if (!owner) return nullptr;

    toy::PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    if (!phys) return nullptr;

    std::vector<toy::ColliderComponent*> enemies;
    phys->GetCollidersByFlag(C_ENEMY_TEAM, enemies);

    const Vector3 selfPos = owner->GetPosition();

    float bestDistSq = std::numeric_limits<float>::max();
    toy::Actor* best = nullptr;

    for (auto* col : enemies)
    {
        auto* a = col->GetOwner();
        if (!a || a == owner) continue;

        Vector3 d = a->GetPosition() - selfPos;
        float d2 = d.LengthSq();

        if (d2 < bestDistSq)
        {
            bestDistSq = d2;
            best = a;
        }
    }
    return best;
}
} // namespace toy
