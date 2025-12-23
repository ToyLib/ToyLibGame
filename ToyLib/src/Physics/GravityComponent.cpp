#include "Physics/GravityComponent.h"
#include "Engine/Core/Actor.h"
#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Physics/PhysWorld.h"
#include "Engine/Core/Application.h"

#include <limits>

namespace toy {

//------------------------------------------------------------------------------
// GravityComponent
//------------------------------------------------------------------------------
// ・Y方向の速度(mVelocityY)を積算し、足元(C_FOOT)基準で地面へスナップする。
// ・重力は「ユニット/秒^2」、速度は「ユニット/秒」。
// ・deltaTime が大きい環境でも破綻しにくいよう、内部でサブステップ更新する。
//------------------------------------------------------------------------------
GravityComponent::GravityComponent(Actor* a)
    : Component(a)
    , mVelocityY(0.0f)
    , mGravityAccel(-80.0f)
    , mJumpSpeed(35.0f)
    , mMaxFallSpeed(-40.0f)
    , mMaxStepUp(0.35f)
    , mMaxStepDown(0.75f)
    , mPenetrationEps(0.05f)
    , mEnableGroundPose(false)
    , mIsGrounded(false)
    , mSelfFlag(C_PLAYER_TEAM)
    , mGroundCollider(nullptr)
    , mPrevGroundPos(Vector3::Zero)
{
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・最大 120fps 相当の小ステップに分割し、重力/接地を安定させる。
// ・最後に「上昇中の天井押し戻し」を行う（天井抜け防止）。
//------------------------------------------------------------------------------
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


}

//------------------------------------------------------------------------------
// StepGravityOnce
//------------------------------------------------------------------------------
// ・dt 分だけ重力を進める（サブステップ 1 回分）。
// ・下向き速度のときのみ、地面(hit.y)へスナップを試みる。
// ・スナップできなければ通常の自由落下として pos.y を更新。
//------------------------------------------------------------------------------
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

    // 足元（C_FOOT の AABB.min.y）で地面との距離を見る
    float footY = foot->GetBoundingVolume()->GetWorldAABB().min.y;

    //============================================================
    // ★ 床追従（同じ床に乗っている間は、床の移動Δに追従）
    //   - 上昇床（エレベータ）対策：Yも含めて追従してOK
    //   - 追従後は footY を取り直す
    //============================================================
    if (hasGround &&
        mIsGrounded &&
        hit.source == GroundSource::Collider &&
        hit.collider &&
        hit.collider->HasFlag(C_GROUND) &&
        hit.collider == mGroundCollider)
    {
        // ※親子があるなら GetWorldPosition() 相当に置き換え推奨
        const Vector3 groundPos = hit.collider->GetOwner()->GetPosition();
        const Vector3 deltaG    = groundPos - mPrevGroundPos;

        pos += deltaG;
        owner->SetPosition(pos);

        mPrevGroundPos = groundPos;

        // 追従で足位置が変わるので取り直す
        footY = foot->GetBoundingVolume()->GetWorldAABB().min.y;
    }

    // 予測足位置（自分のY速度による次フレーム）
    const float nextFootY = footY + mVelocityY * deltaTime;

    //============================================================
    // ★ 接地（スナップ）判定
    //   - 通常：落下中(mVelocityY<=0)だけ
    //   - 例外：同じ床に“乗り続けている”間は上昇中でも許可（上昇床対策）
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

        // nextFootY に対して groundY がどれだけ上下にあるか
        //  yGapNext > 0 : 地面が高い（段差/坂を拾う）
        //  yGapNext < 0 : 地面が低い（落下して着地する）
        const float yGapNext = groundY - nextFootY;

        const float upLimit   = mMaxStepUp   + mPenetrationEps;
        const float downLimit = mMaxStepDown + mPenetrationEps;

        const bool canSnap =
            (yGapNext >= -downLimit) &&
            (yGapNext <=  upLimit);

        if (canSnap)
        {
            //--------------------------------------------------------
            // 床の“乗り換え”検出（初回だけ prev を同期）
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
                    // 同じ床なら prev は床追従部で更新済みのはずだが、
                    // 念のため同期しておく（差分ゼロになってもOK）
                    mPrevGroundPos = groundPos;
                }
            }
            else
            {
                mGroundCollider = nullptr;
            }

            //--------------------------------------------------------
            // ★ Yだけスナップ（XZは保持して歩行を潰さない）
            //--------------------------------------------------------
            // 追従や他処理で足位置が変わっている可能性があるので再取得
            footY = foot->GetBoundingVolume()->GetWorldAABB().min.y;

            Vector3 cur = owner->GetPosition();
            const float offsetY = cur.y - footY;
            cur.y = groundY + offsetY + 0.001f;
            owner->SetPosition(cur);

            mVelocityY  = 0.0f;
            mIsGrounded = true;

            if (mEnableGroundPose)
            {
                ApplyGroundPose(owner, hit, deltaTime);
            }
            return;
        }
    }

    //============================================================
    // 接地しなかった（空中）
    //============================================================
    mIsGrounded = false;
    mGroundCollider = nullptr;

    // 自由落下（Yのみ）
    pos = owner->GetPosition();
    pos.y += mVelocityY * deltaTime;
    owner->SetPosition(pos);
}
//------------------------------------------------------------------------------
// ApplyCeilingClamp
//------------------------------------------------------------------------------
// ・上昇中のみ、天井(C_CEILING)にめり込んだら押し戻して上向き速度を止める。
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// ApplyGroundPose
//------------------------------------------------------------------------------
// ・地面法線(hit.normal)に合わせた「ポーズ回転」を作り、Slerp で滑らかに補間。
// ・ワールドの normal を、Actor のローカル基準へ落としてから Up と比較する。
//------------------------------------------------------------------------------
void GravityComponent::ApplyGroundPose(Actor* owner, const GroundHit& hit, float deltaTime)
{
    // 1) ワールド normal → ローカル normal
    Quaternion inv = owner->GetRotation();
    inv.Conjugate();

    Vector3 localNormal = Vector3::Transform(hit.normal, inv);
    if (localNormal.LengthSq() < Math::NearZeroEpsilon)
    {
        return;
    }
    localNormal.Normalize();

    // 2) ローカル Up と比較
    const Vector3 localUp = Vector3::UnitY;

    float dot = Vector3::Dot(localUp, localNormal);
    dot = Math::Clamp(dot, -1.0f, 1.0f);

    const float angle = Math::Acos(dot);

    // 3) 目標ポーズ
    Quaternion targetPose = Quaternion::Identity;

    if (angle >= 0.001f)
    {
        Vector3 axis = Vector3::Cross(localUp, localNormal);
        if (axis.LengthSq() > Math::NearZeroEpsilon)
        {
            axis.Normalize();
            targetPose = Quaternion(axis, angle); // angle は rad 前提
        }
    }

    // 4) 補間（速度は好みで調整）
    const float poseLerpSpeed = 12.0f;
    const float t = Math::Clamp(deltaTime * poseLerpSpeed, 0.0f, 1.0f);

    const Quaternion currentPose = owner->GetPoseRotation();
    const Quaternion smoothPose  = Quaternion::Slerp(currentPose, targetPose, t);

    owner->SetPoseRotation(smoothPose);
}

void GravityComponent::ApplyGroundDepenetration(Actor* owner, ColliderComponent* foot)
{
    if (!owner)
    {
        return;
    }
    if (!foot)
    {
        return;
    }

    PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    if (!phys)
    {
        return;
    }

    GroundHit hit;
    if (!phys->GetNearestGroundHit(owner, hit))
    {
        return;
    }

    // 足元のAABB min.y が「足の高さ基準」
    const Cube box   = foot->GetBoundingVolume()->GetWorldAABB();
    const float footY = box.min.y;

    // yGap = footY - groundY
    const float yGap = footY - hit.y;

    // 足が地面より下（潜っている）なら引き上げる
    // ここは “保険” なので、過剰に動かさず eps を超えたらだけ直す
    if (yGap < -mPenetrationEps)
    {
        Vector3 pos = owner->GetPosition();

        // Actor原点→足元のオフセットを維持したまま、足を groundY に乗せる
        const float offset = pos.y - footY;
        pos.y = hit.y + offset + 0.001f;
        owner->SetPosition(pos);

        // 下向き速度は止めておく（落下中の潜り救済）
        if (mVelocityY < 0.0f)
        {
            mVelocityY = 0.0f;
        }

        mIsGrounded = true;

        if (mEnableGroundPose)
        {
            // dt が無いので “軽く追従” させたい場合は固定値でもOK
            // ここでは Update() 側から dt を渡す版にしてもいい
            // ApplyGroundPose(owner, hit, /*dt=*/0.016f);
        }
    }
}

//------------------------------------------------------------------------------
// Jump
//------------------------------------------------------------------------------
// ・接地中のみジャンプ可能。
//------------------------------------------------------------------------------
void GravityComponent::Jump()
{
    if (mIsGrounded)
    {
        mVelocityY  = mJumpSpeed;
        mIsGrounded = false;
    }
}

//------------------------------------------------------------------------------
// FindFootCollider
//------------------------------------------------------------------------------
// ・Owner が持つ ColliderComponent のうち、C_FOOT を持つものを返す。
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
