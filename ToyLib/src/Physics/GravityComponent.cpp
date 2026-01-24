//======================================================================
// GravityComponent.cpp
//======================================================================
#include "Physics/GravityComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Physics/PhysWorld.h"

namespace toy {

//------------------------------------------------------------------------------
// ヘルパー：Actor の位置を更新し、BV / OBB / AABB を同期
//------------------------------------------------------------------------------
inline void SetOwnerPosAndSync(Actor* owner, const Vector3& pos)
{
    owner->SetPosition(pos);
    owner->ComputeWorldTransform();
}

//------------------------------------------------------------------------------
// ctor
//------------------------------------------------------------------------------
GravityComponent::GravityComponent(Actor* a)
    : Component(a)
    , mSelfFlag(C_BODY)
    , mVelocityY(0.0f)
    , mGravityAccel(-60.0f)
    , mJumpSpeed(22.0f)
    , mMaxFallSpeed(-40.0f)
    , mIsGrounded(false)
    , mEnableGroundPose(false)
    , mMaxStepUp(0.35f)
    , mMaxStepDown(0.75f)
    , mPenetrationEps(0.05f)
    , mGroundCollider(nullptr)
    , mPrevGroundPos(Vector3::Zero)
{
}

//------------------------------------------------------------------------------
// Update
// ・deltaTime をサブステップに分割して安定した重力・接地判定を行う
//------------------------------------------------------------------------------
void GravityComponent::Update(float dt)
{
    ColliderComponent* foot = FindFootCollider();
    if (!foot)
    {
        return;
    }

    // 自己フラグが未設定なら FOOT として扱う
    if (mSelfFlag == 0)
    {
        mSelfFlag = C_FOOT;
    }

    float remaining = dt;
    const float kMaxStep = 1.0f / 120.0f;

    while (remaining > 0.0f)
    {
        float step = std::min(remaining, kMaxStep);

        StepGravityOnce(step, foot);
        ApplyCeilingClamp(GetOwner());

        remaining -= step;
    }
}

//------------------------------------------------------------------------------
// StepGravityOnce
// ・サブステップ 1 回分の重力・接地・床追従処理
//------------------------------------------------------------------------------
void GravityComponent::StepGravityOnce(float deltaTime, ColliderComponent* foot)
{
    Actor* owner    = GetOwner();
    PhysWorld* phys = owner ? owner->GetApp()->GetPhysWorld() : nullptr;
    if (!owner || !phys || !foot)
    {
        return;
    }

    const bool wasGrounded = mIsGrounded;

    //==========================================================================
    // Pre) 動く床への追従
    //==========================================================================
    if (mIsGrounded && mGroundCollider && mGroundCollider->GetEnabled())
    {
        Actor* groundOwner = mGroundCollider->GetOwner();
        if (groundOwner)
        {
            const Vector3 groundPos = groundOwner->GetPosition();
            const Vector3 deltaG    = groundPos - mPrevGroundPos;

            if (deltaG.LengthSq() > 1e-12f)
            {
                Vector3 pos = owner->GetPosition();
                pos += deltaG;
                SetOwnerPosAndSync(owner, pos);
            }

            mPrevGroundPos = groundPos;
        }
    }

    //==========================================================================
    // 0) 現在の足位置（OBB 最下点 + 中心XZ）
    //==========================================================================
    auto obbPtr = foot->GetBoundingVolume()->GetOBB();
    if (!obbPtr)
    {
        return;
    }

    const OBB& footObb = *obbPtr;

    // OBB の「最下点Y」を support 的に計算（傾き考慮）
    auto GetOBBMinY = [](const OBB& obb) -> float
    {
        const float dy =
            std::fabs(obb.axisX.y) * obb.radius.x +
            std::fabs(obb.axisY.y) * obb.radius.y +
            std::fabs(obb.axisZ.y) * obb.radius.z;

        return obb.pos.y - dy;
    };

    const float footY0 = GetOBBMinY(footObb);

    // 足の検知点は「中心XZ」で十分（Yは使わないので0でもOK）
    const Vector3 footCenter0(footObb.pos.x, 0.0f, footObb.pos.z);

    // このサブステップ内では「足 → Actor」のオフセットを固定する
    const Vector3 ownerPos0 = owner->GetPosition();
    const float offsetY0    = ownerPos0.y - footY0;

    //==========================================================================
    // 1) 真下の床を取得（Collider 床 + Terrain）
    //==========================================================================
    const float kStartUp  = 0.05f;
    const float kExtraRay = 0.5f;

    const Vector3 origin0(footCenter0.x, footY0 + kStartUp, footCenter0.z);
    const float rayLen = (mMaxStepDown + 2.0f) + kExtraRay;

    GroundHit hit;
    bool hasGround = phys->GetGroundHitRayDown(origin0,
                                               rayLen,
                                               C_GROUND,
                                               hit);

    // 動く床が上がった場合など、明らかに高すぎるヒットは破棄
    if (hasGround)
    {
        const float kAllowUp = (mMaxStepUp + mPenetrationEps) + 0.02f;
        if (hit.y > origin0.y + kAllowUp)
        {
            hasGround = false;
        }
    }

    // 法線の反転防止（フレーム間の安定化）
    if (hasGround)
    {
        if (Vector3::Dot(hit.normal, mPrevGroundNormal) < 0.0f)
        {
            hit.normal *= -1.0f;
        }
        mPrevGroundNormal = hit.normal;
    }

    //==========================================================================
    // 2) 地面姿勢キャッシュ更新（取得できたら常に更新）
    //==========================================================================
    if (hasGround)
    {
        UpdateGroundPoseCache(owner, hit, deltaTime, mIsGrounded, wasGrounded);
    }
    else
    {
        mGroundPose.valid    = false;
        mGroundPose.grounded = false;
        mGroundPose.collider = nullptr;
    }

    //==========================================================================
    // 3) 床追従（レイで同一床が取れた場合の整合）
    //==========================================================================
    if (hasGround &&
        mIsGrounded &&
        hit.source == GroundSource::Collider &&
        hit.collider &&
        hit.collider->HasFlag(C_GROUND) &&
        hit.collider == mGroundCollider)
    {
        const Vector3 groundPos = hit.collider->GetOwner()->GetPosition();
        const Vector3 deltaG    = groundPos - mPrevGroundPos;

        Vector3 pos = owner->GetPosition();
        pos += deltaG;
        SetOwnerPosAndSync(owner, pos);

        mPrevGroundPos = groundPos;
    }

    //==========================================================================
    // 4) 接地中の貼り付き処理（重力を入れない）
    //==========================================================================
    if (mIsGrounded && hasGround)
    {
        const float groundY = hit.y;
        const float gap     = footY0 - groundY; // ★ foot最下点基準

        const float kStickUp   = 0.03f;
        const float kStickDown = mPenetrationEps + 0.02f;

        // (A) 通常の貼り付き
        if (gap <= kStickUp && gap >= -kStickDown)
        {
            Vector3 cur = owner->GetPosition();
            cur.y = groundY + offsetY0 + 0.001f;
            SetOwnerPosAndSync(owner, cur);

            mVelocityY  = 0.0f;
            mIsGrounded = true;

            UpdateGroundPoseCache(owner, hit, deltaTime, true, wasGrounded);
            if (mEnableGroundPose)
            {
                owner->SetPoseRotation(mGroundPose.smooth);
            }
            return;
        }

        // (B) 動く床が上昇した場合の押し上げ
        if (gap < -kStickDown)
        {
            const float lift    = -gap;
            const float kLiftUp = (mMaxStepUp + mPenetrationEps) + 0.02f;
            if (lift <= kLiftUp)
            {
                Vector3 cur = owner->GetPosition();
                cur.y = groundY + offsetY0 + 0.001f;
                SetOwnerPosAndSync(owner, cur);

                mVelocityY  = 0.0f;
                mIsGrounded = true;

                UpdateGroundPoseCache(owner, hit, deltaTime, true, wasGrounded);
                if (mEnableGroundPose)
                {
                    owner->SetPoseRotation(mGroundPose.smooth);
                }
                return;
            }
        }
    }

    //==========================================================================
    // 5) 空中：重力加速
    //==========================================================================
    mVelocityY += mGravityAccel * deltaTime;
    if (mVelocityY < mMaxFallSpeed)
    {
        mVelocityY = mMaxFallSpeed;
    }

    const float nextFootY = footY0 + mVelocityY * deltaTime; // ★ foot最下点基準

    //==========================================================================
    // 6) 落下中の着地スナップ判定
    //==========================================================================
    if (hasGround && mVelocityY <= 0.0f)
    {
        const float groundY = hit.y;

        const float upLimit   = mMaxStepUp   + mPenetrationEps;
        const float downLimit = mMaxStepDown + mPenetrationEps;

        const bool  withinUp  = (groundY <= footY0 + upLimit); // ★ foot最下点基準
        const float yGapNext  = groundY - nextFootY;
        const bool  withinDown = (yGapNext >= -downLimit);

        if (withinUp && withinDown)
        {
            if (hit.source == GroundSource::Collider &&
                hit.collider &&
                hit.collider->HasFlag(C_GROUND))
            {
                const Vector3 groundPos = hit.collider->GetOwner()->GetPosition();
                mGroundCollider = hit.collider;
                mPrevGroundPos  = groundPos;
            }
            else
            {
                mGroundCollider = nullptr;
            }

            Vector3 cur = owner->GetPosition();
            cur.y = groundY + offsetY0 + 0.001f;
            SetOwnerPosAndSync(owner, cur);

            mVelocityY  = 0.0f;
            mIsGrounded = true;

            UpdateGroundPoseCache(owner, hit, deltaTime, true, wasGrounded);
            if (mEnableGroundPose)
            {
                owner->SetPoseRotation(mGroundPose.smooth);
            }
            return;
        }
    }

    //==========================================================================
    // 7) 自由落下
    //==========================================================================
    mIsGrounded     = false;
    mGroundCollider = nullptr;

    mGroundPose.valid    = hasGround;
    mGroundPose.grounded = false;

    Vector3 pos = owner->GetPosition();
    pos.y += mVelocityY * deltaTime;
    SetOwnerPosAndSync(owner, pos);
}

//------------------------------------------------------------------------------
// 天井衝突処理（上昇中のみ）
//------------------------------------------------------------------------------
void GravityComponent::ApplyCeilingClamp(Actor* owner)
{
    if (!owner || mVelocityY <= 0.0f)
    {
        return;
    }

    PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    if (!phys)
    {
        return;
    }

    Vector3 push = Vector3::Zero;

    if (phys->ResolveCeiling(owner, mSelfFlag, C_CEILING, push))
    {
        owner->SetPosition(owner->GetPosition() + push);
        owner->ComputeWorldTransform();
        mVelocityY = 0.0f;
    }
}

//------------------------------------------------------------------------------
// 地面姿勢キャッシュ更新
//------------------------------------------------------------------------------
void GravityComponent::UpdateGroundPoseCache(Actor* owner,
                                             const GroundHit& hit,
                                             float deltaTime,
                                             bool groundedNow,
                                             bool wasGrounded)
{
    if (!owner)
    {
        return;
    }

    mGroundPose.valid    = true;
    mGroundPose.grounded = groundedNow;
    mGroundPose.y        = hit.y;
    mGroundPose.normal   = hit.normal;
    mGroundPose.collider = hit.collider;

    Quaternion inv = owner->GetRotation();
    inv.Conjugate();

    Vector3 localNormal = Vector3::Transform(hit.normal, inv);
    if (localNormal.LengthSq() < Math::NearZeroEpsilon)
    {
        mGroundPose.raw = Quaternion::Identity;
        return;
    }
    localNormal.Normalize();

    const Vector3 localUp = Vector3::UnitY;
    float dot = Math::Clamp(Vector3::Dot(localUp, localNormal), -1.0f, 1.0f);
    const float angle = Math::Acos(dot);

    Quaternion target = Quaternion::Identity;
    if (angle >= 0.001f)
    {
        Vector3 axis = Vector3::Cross(localUp, localNormal);
        if (axis.LengthSq() > Math::NearZeroEpsilon)
        {
            axis.Normalize();
            target = Quaternion(axis, angle);
        }
    }

    mGroundPose.raw = target;

    const float poseLerpSpeed = 12.0f;
    const float t = Math::Clamp(deltaTime * poseLerpSpeed, 0.0f, 1.0f);

    if (!wasGrounded && groundedNow)
    {
        mGroundPose.smooth = target;
    }
    else
    {
        mGroundPose.smooth = Quaternion::Slerp(mGroundPose.smooth, target, t);
    }
}

//------------------------------------------------------------------------------
// C_FOOT Collider 探索
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

//------------------------------------------------------------------------------
// Jump
// ・接地中のみジャンプ可能
// ・上向き初速を与えて空中状態に移行する
//------------------------------------------------------------------------------
void GravityComponent::Jump()
{
    if (!mIsGrounded)
    {
        return;
    }

    mVelocityY  = mJumpSpeed;
    mIsGrounded = false;

    // 接地フラグだけ落とす。
    // 真下に地面があれば GroundPose は引き続き更新される。
    mGroundPose.grounded = false;
}

} // namespace toy
