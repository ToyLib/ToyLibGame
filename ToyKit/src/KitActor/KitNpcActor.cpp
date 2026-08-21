#include "KitActor/KitNpcActor.h"

#include <cmath>

namespace toy::kit {

//-----------------------------------------------------------------------------
KitNpcActor::KitNpcActor(toy::Application* a)
    : KitCharacterActor(a)
{
}

//-----------------------------------------------------------------------------
bool KitNpcActor::HasTarget() const
{
    return mTarget != nullptr;
}

//-----------------------------------------------------------------------------
// XZ 平面上の距離（高さ差は無視）
//-----------------------------------------------------------------------------
float KitNpcActor::GetDistanceToTarget() const
{
    if (!mTarget) return 1.0e9f;

    const Vector3& self   = GetPosition();
    const Vector3& target = mTarget->GetPosition();
    float dx = target.x - self.x;
    float dz = target.z - self.z;
    return sqrtf(dx * dx + dz * dz);
}

//-----------------------------------------------------------------------------
// XZ 平面でターゲット方向に移動する。Y は GravityComponent に任せる。
//-----------------------------------------------------------------------------
void KitNpcActor::MoveTowardTarget(float speed, float dt)
{
    if (!mTarget) return;

    const Vector3& self   = GetPosition();
    const Vector3& target = mTarget->GetPosition();
    float dx   = target.x - self.x;
    float dz   = target.z - self.z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist < mStopRange) return;

    float step = speed * dt / dist;
    SetPosition(Vector3(self.x + dx * step,
                        self.y,               // Y は重力に任せる
                        self.z + dz * step));
}

//-----------------------------------------------------------------------------
// ターゲット方向に Y 軸回転する
//-----------------------------------------------------------------------------
void KitNpcActor::LookAtTarget()
{
    if (!mTarget) return;

    const Vector3& self   = GetPosition();
    const Vector3& target = mTarget->GetPosition();
    float dx = target.x - self.x;
    float dz = target.z - self.z;
    if (dx * dx + dz * dz < 0.01f) return;

    float angle = atan2f(-dx, -dz);
    SetRotation(Quaternion(Vector3::UnitY, angle));
}

} // namespace toy::kit
