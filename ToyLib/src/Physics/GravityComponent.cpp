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

GravityComponent::GravityComponent(Actor* a)
    : Component(a)
{
    // 既定値はヘッダ側のメンバ初期化に任せる
}

void GravityComponent::Update(float deltaTime)
{
    ColliderComponent* foot = FindFootCollider();
    if (!foot)
    {
        return;
    }

    float remaining = deltaTime;
    const float kMaxStep = 1.0f / 120.0f;

    while (remaining > 0.0f)
    {
        float step = (remaining > kMaxStep) ? kMaxStep : remaining;
        StepGravityOnce(step, foot);
        remaining -= step;
    }

    ApplyCeilingClamp(GetOwner());

    // 保険（必要なら有効化）
    // ApplyGroundDepenetration(GetOwner(), foot);
}

void GravityComponent::StepGravityOnce(float deltaTime, ColliderComponent* foot)
{
    Actor* owner    = GetOwner();
    PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    Vector3 pos     = owner->GetPosition();

    // 1) 加速度 → 速度（終端速度でクランプ）
    mVelocityY += mGravityAccel * deltaTime;
    if (mVelocityY < mMaxFallSpeed)
    {
        mVelocityY = mMaxFallSpeed;
    }

    // 真下方向の地面（Collider床 + Terrain の「最も高い地面」）
    GroundHit hit;
    const bool hasGround = phys->GetNearestGroundHit(owner, hit);

    // 足元（C_FOOT の AABB.min.y）
    float footY = foot->GetBoundingVolume()->GetWorldAABB().min.y;

    //============================================================
    // 床追従（同じ床に乗っている間は、床の移動Δに追従）
    //============================================================
    if (hasGround &&
        mIsGrounded &&
        hit.source == GroundSource::Collider &&
        hit.collider &&
        hit.collider->HasFlag(C_GROUND) &&
        hit.collider == mGroundCollider)
    {
        const Vector3 groundPos = hit.collider->GetOwner()->GetPosition();
        const Vector3 deltaG    = groundPos - mPrevGroundPos;

        pos += deltaG;
        owner->SetPosition(pos);

        mPrevGroundPos = groundPos;

        // 追従で足位置が変わるので取り直す
        footY = foot->GetBoundingVolume()->GetWorldAABB().min.y;
    }

    // 予測足位置
    const float nextFootY = footY + mVelocityY * deltaTime;

    //============================================================
    // 接地（スナップ）判定
    //============================================================
    const bool allowSnap =
        (mVelocityY <= 0.0f) ||
        (mIsGrounded &&
         hasGround &&
         hit.source == GroundSource::Collider &&
         hit.collider &&
         hit.collider == mGroundCollider);

    if (hasGround && allowSnap)
    {
        const float groundY = hit.y;

        const float yGapNext = groundY - nextFootY;

        const float upLimit   = mMaxStepUp   + mPenetrationEps;
        const float downLimit = mMaxStepDown + mPenetrationEps;

        const bool canSnap =
            (yGapNext >= -downLimit) &&
            (yGapNext <=  upLimit);

        if (canSnap)
        {
            //--------------------------------------------------------
            // 床の“乗り換え”検出
            //--------------------------------------------------------
            if (hit.source == GroundSource::Collider &&
                hit.collider &&
                hit.collider->HasFlag(C_GROUND))
            {
                const Vector3 groundPos = hit.collider->GetOwner()->GetPosition();

                if (hit.collider != mGroundCollider)
                {
                    mGroundCollider = hit.collider;
                    mPrevGroundPos  = groundPos;
                }
                else
                {
                    mPrevGroundPos = groundPos;
                }
            }
            else
            {
                mGroundCollider = nullptr;
            }

            //--------------------------------------------------------
            // Yだけスナップ（XZは保持）
            //--------------------------------------------------------
            footY = foot->GetBoundingVolume()->GetWorldAABB().min.y;

            Vector3 cur = owner->GetPosition();
            const float offsetY = cur.y - footY;
            cur.y = groundY + offsetY + 0.001f;
            owner->SetPosition(cur);

            mVelocityY  = 0.0f;
            mIsGrounded = true;

            //--------------------------------------------------------
            // ★ 地面姿勢は「常に」計算してキャッシュ
            //--------------------------------------------------------
            UpdateGroundPoseCache(owner, hit, deltaTime);

            //--------------------------------------------------------
            // ★ Actorへの反映は必要時だけ
            //--------------------------------------------------------
            if (mEnableGroundPose)
            {
                owner->SetPoseRotation(mGroundPose.smooth);
            }

            return;
        }
    }

    //============================================================
    // 接地しなかった（空中）
    //============================================================
    mIsGrounded = false;
    mGroundCollider = nullptr;

    // ground pose は無効化（誤使用防止）
    mGroundPose.valid    = false;
    mGroundPose.grounded = false;
    mGroundPose.collider = nullptr;

    // 自由落下（Yのみ）
    pos = owner->GetPosition();
    pos.y += mVelocityY * deltaTime;
    owner->SetPosition(pos);
}

void GravityComponent::ApplyCeilingClamp(Actor* owner)
{
    if (!owner) return;

    // 上昇中のみ
    if (mVelocityY <= 0.0f)
    {
        return;
    }

    auto* phys = owner->GetApp()->GetPhysWorld();

    Vector3 push = Vector3::Zero;
    if (phys->ResolveCeiling(owner, mSelfFlag, C_CEILING, push))
    {
        owner->SetPosition(owner->GetPosition() + push);

        // 下向きに押された = 天井ヒット
        if (push.y < -0.0001f)
        {
            mVelocityY = 0.0f;
        }
    }
}

void GravityComponent::UpdateGroundPoseCache(Actor* owner, const GroundHit& hit, float deltaTime)
{
    if (!owner) return;

    mGroundPose.valid    = true;
    mGroundPose.grounded = true;
    mGroundPose.y        = hit.y;
    mGroundPose.normal   = hit.normal;
    mGroundPose.collider = hit.collider; // Collider床じゃなければ nullptr でもOK

    // -----------------------------
    // ApplyGroundPose の「計算部分」を移植
    // -----------------------------

    // 1) ワールド normal → ローカル normal（Actor回転基準）
    Quaternion inv = owner->GetRotation();
    inv.Conjugate();

    Vector3 localNormal = Vector3::Transform(hit.normal, inv);
    if (localNormal.LengthSq() < Math::NearZeroEpsilon)
    {
        mGroundPose.raw = Quaternion::Identity;
        // smooth は据え置き（急にIdentityに戻すと見た目が跳ねる）
        return;
    }
    localNormal.Normalize();

    // 2) ローカル Up と比較
    const Vector3 localUp = Vector3::UnitY;

    float dot = Vector3::Dot(localUp, localNormal);
    dot = Math::Clamp(dot, -1.0f, 1.0f);

    const float angle = Math::Acos(dot);

    // 3) 目標ポーズ（raw）
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

    // 4) 補間（smooth）
    const float poseLerpSpeed = 12.0f;
    const float t = Math::Clamp(deltaTime * poseLerpSpeed, 0.0f, 1.0f);

    // 初回接地時：smooth を raw に同期して跳ねを減らす
    if (!mIsGrounded)
    {
        mGroundPose.smooth = target;
    }
    else
    {
        mGroundPose.smooth = Quaternion::Slerp(mGroundPose.smooth, target, t);
    }
}

void GravityComponent::ApplyGroundDepenetration(Actor* owner, ColliderComponent* foot)
{
    if (!owner || !foot) return;

    PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    if (!phys) return;

    GroundHit hit;
    if (!phys->GetNearestGroundHit(owner, hit))
    {
        return;
    }

    const Cube box   = foot->GetBoundingVolume()->GetWorldAABB();
    const float footY = box.min.y;

    const float yGap = footY - hit.y;

    if (yGap < -mPenetrationEps)
    {
        Vector3 pos = owner->GetPosition();

        const float offset = pos.y - footY;
        pos.y = hit.y + offset + 0.001f;
        owner->SetPosition(pos);

        if (mVelocityY < 0.0f)
        {
            mVelocityY = 0.0f;
        }

        mIsGrounded = true;

        // ここで ground pose を更新したいなら dt が必要なので、
        // Update() から呼ぶ版にするか、固定dtで更新する
        // UpdateGroundPoseCache(owner, hit, 1.0f/60.0f);
        // if (mEnableGroundPose) owner->SetPoseRotation(mGroundPose.smooth);
    }
}

void GravityComponent::Jump()
{
    if (mIsGrounded)
    {
        mVelocityY  = mJumpSpeed;
        mIsGrounded = false;

        // ジャンプした瞬間は ground pose は無効化しておくと誤参照を防げる
        mGroundPose.valid    = false;
        mGroundPose.grounded = false;
        mGroundPose.collider = nullptr;
    }
}

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
