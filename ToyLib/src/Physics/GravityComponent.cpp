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
    , mSelfFlag(C_PLAYER) // ★ 汎用化したいなら外から設定できるようにする
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

/*    // 壁めり込み保険（水平押し戻し）
    if (auto* phys = GetOwner()->GetApp()->GetPhysWorld())
    {
        Vector3 wallPush = Vector3::Zero;

        if (phys->ResolveWallPenetration(GetOwner(), mSelfFlag, C_WALL, wallPush))
        {
            // ResolveWallPenetration 内で一時的に位置を動かしているので、
            // ここで戻したい派なら「outPushを返すだけ」にして適用は外側に寄せてもOK。
            // ただ、今の実装は“保険優先”で内側で更新する方式。
            (void)wallPush;
        }
    }
    ApplyGroundDepenetration(GetOwner(), foot);
*/
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

    // 足元（C_FOOT の AABB.min.y）で地面との距離を見る
    const float footY     = foot->GetBoundingVolume()->GetWorldAABB().min.y;
    const float nextFootY = footY + mVelocityY * deltaTime;

    // 真下方向の地面（Collider床 + Terrain の「最も高い地面」）
    GroundHit hit;
    const bool hasGround = phys->GetNearestGroundHit(owner, hit);

    // 下向きのときだけ接地（スナップ）判定
    if (hasGround && mVelocityY <= 0.0f)
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
            // Actor原点 → 足元までのオフセットを維持して、足元を groundY に合わせる
            const float offset = pos.y - footY;
            pos.y = groundY + offset + 0.001f; // ほんの少し浮かせて誤差めり込み対策
            owner->SetPosition(pos);

            mVelocityY  = 0.0f;
            mIsGrounded = true;

            if (mEnableGroundPose)
            {
                ApplyGroundPose(owner, hit, deltaTime);
            }
            return;
        }
    }

    // 今回は接地しなかった
    mIsGrounded = false;

    // 2) 自由落下
    pos.y += mVelocityY * deltaTime;
    owner->SetPosition(pos);

    // 空中時に姿勢を戻したいならここで Identity へ補間してもOK（任意）
    // if (mEnableGroundPose) { ApplyAirPoseReturn(owner, dt); }
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
