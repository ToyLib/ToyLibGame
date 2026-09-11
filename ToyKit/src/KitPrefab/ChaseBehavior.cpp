#include "KitPrefab/ChaseBehavior.h"
#include "KitPrefab/Prefab.h"

#include <cmath>

namespace toy::kit {

//-----------------------------------------------------------------------------
void ChaseBehavior::OnStart(Prefab& body)
{
    mBody = &body;

    // Idle : 索敵して Chase へ
    // Chase: 追跡。見失ったら Idle へ（ヒステリシス）
    mFSM.Register(State::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget() && GetDistanceToTarget() < mDesc.detectRange)
            {
                mFSM.To(State::Chase);
            }
        },
        /* onEnter */ [this] { mBody->PlayAnimationBlend(mDesc.idleAnim, mDesc.animBlendSec); }
    );

    mFSM.Register(State::Chase,
        /* onUpdate */ [this](float dt)
        {
            MoveTowardTarget(dt);
            LookAtTarget();
            if (!HasTarget() || GetDistanceToTarget() > mDesc.detectRange * mDesc.loseRangeMultiplier)
            {
                mFSM.To(State::Idle);
            }
        },
        /* onEnter */ [this] { mBody->PlayAnimationBlend(mDesc.chaseAnim, mDesc.animBlendSec); }
    );

    mFSM.Start(State::Idle);
}

//-----------------------------------------------------------------------------
void ChaseBehavior::OnUpdate(Prefab& /*body*/, float deltaTime)
{
    mFSM.Update(deltaTime);
}

//-----------------------------------------------------------------------------
float ChaseBehavior::GetDistanceToTarget() const
{
    if (!mTarget) return 1.0e9f;

    const Vector3& self   = mBody->GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx = target.x - self.x;
    const float dz = target.z - self.z;
    return sqrtf(dx * dx + dz * dz);
}

//-----------------------------------------------------------------------------
void ChaseBehavior::MoveTowardTarget(float dt)
{
    if (!mTarget) return;

    const Vector3& self   = mBody->GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx   = target.x - self.x;
    const float dz   = target.z - self.z;
    const float dist = sqrtf(dx * dx + dz * dz);
    if (dist < mDesc.stopRange) return;

    const float step = mDesc.moveSpeed * dt / dist;
    mBody->SetPosition(Vector3(self.x + dx * step,
                               self.y,               // Y は重力に任せる
                               self.z + dz * step));
}

//-----------------------------------------------------------------------------
void ChaseBehavior::LookAtTarget()
{
    if (!mTarget) return;

    const Vector3& self   = mBody->GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx = target.x - self.x;
    const float dz = target.z - self.z;
    if (dx * dx + dz * dz < 0.01f) return;

    const float angle = atan2f(-dx, -dz);
    mBody->SetRotation(Quaternion(Vector3::UnitY, angle));
}

} // namespace toy::kit
