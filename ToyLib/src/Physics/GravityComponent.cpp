#include "Physics/GravityComponent.h"
#include "Engine/Core/Actor.h"
#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Physics/PhysWorld.h"
#include "Engine/Core/Application.h"
#include <limits>

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
// ・初期Y速度は 0。
// ・重力加速度はデフォルトで -2.8f（ゲーム全体で調整する前提）。
// ・ジャンプ初速は 50.0f。
//------------------------------------------------------------------------------
GravityComponent::GravityComponent(Actor* a)
: Component(a)
, mVelocityY(0.0f)
, mGravityAccel(-80.0f)   // 例：秒^2
, mJumpSpeed(35.0f)       // 例：秒
, mMaxFallSpeed(-40.0f)   // ★ 秒あたり -40 ユニットまでしか落ちない
, mIsGrounded(false)
{
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・mVelocityY に重力加速度を加算して位置更新。
// ・PhysWorld::GetNearestGroundY() を使って足元の最も近い地面Yを取得。
// ・足コライダー（C_FOOT）AABB の min.y と groundY を比較して、
//   次フレームの足元が groundY を下回る場合は接地とみなし、
//   Y位置を補正して mVelocityY を 0 / mIsGrounded = true にする。
//------------------------------------------------------------------------------
void GravityComponent::Update(float deltaTime)
{
    ColliderComponent* collider = FindFootCollider();
    if (!collider) return;

    Actor* owner    = GetOwner();
    PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    Vector3 pos     = owner->GetPosition();

    // 1) 重力（秒ベース）
    mVelocityY += mGravityAccel * deltaTime;

    // ★ 落下速度の上限（終端速度）を適用
    if (mVelocityY < mMaxFallSpeed)
    {
        mVelocityY = mMaxFallSpeed;
    }

    // ここから下は、さっき一緒に調整した接地判定ロジックでOK
    // （nextFootY 計算して、groundY との yGap を見てスナップ）
    float footY     = collider->GetBoundingVolume()->GetWorldAABB().min.y;
    float nextFootY = footY + mVelocityY * deltaTime;

    float groundY   = -std::numeric_limits<float>::max();
    bool hasGround  = phys->GetNearestGroundY(owner, groundY);

    const float kMaxStepUp      = 0.30f;
    const float kMaxStepDown    = 0.50f;
    const float kPenetrationEps = 0.05f;

    if (hasGround && mVelocityY <= 0.0f)
    {
        float yGapNext = groundY - nextFootY;

        bool canSnap =
            (yGapNext >= -kMaxStepDown - kPenetrationEps) &&
            (yGapNext <=  kMaxStepUp   + kPenetrationEps);

        if (canSnap)
        {
            float offset = pos.y - footY;
            pos.y = groundY + offset + 0.001f;
            owner->SetPosition(pos);

            mVelocityY  = 0.0f;
            mIsGrounded = true;
            return;
        }
    }

    mIsGrounded = false;

    pos.y += mVelocityY * deltaTime;
    owner->SetPosition(pos);
}
//------------------------------------------------------------------------------
// Jump
//------------------------------------------------------------------------------
// ・接地中（mIsGrounded == true）のときだけジャンプ初速を与える。
// ・ジャンプ後は mIsGrounded を false にする。
//------------------------------------------------------------------------------
void GravityComponent::Jump()
{
    if (mIsGrounded)
    {
        mVelocityY = mJumpSpeed;
        mIsGrounded = false;
    }
}

//------------------------------------------------------------------------------
// FindFootCollider
//------------------------------------------------------------------------------
// ・Actor に紐づく ColliderComponent 群から、C_FOOT フラグを持つものを探す。
// ・足用 Collider（カプセルや小さめAABB）を用意しておき、
//   それを地面判定専用に使う前提のヘルパー。
//------------------------------------------------------------------------------
ColliderComponent* GravityComponent::FindFootCollider()
{
    for (auto* comp : GetOwner()->GetAllComponents<ColliderComponent>())
    {
        if (comp->HasFlag(C_FOOT))
        {
            return comp;
        }
    }
    return nullptr;
}

} // namespace toy
