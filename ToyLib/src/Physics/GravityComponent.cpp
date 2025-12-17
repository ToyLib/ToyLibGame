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
// ・Y方向速度は 0 から開始。
// ・mGravityAccel は「ユニット/秒^2」として扱う重力加速度。
// ・mJumpSpeed は「ユニット/秒」として扱うジャンプ初速。
// ・mMaxFallSpeed は「落下方向（負）の終端速度」。
// ・mMaxStepUp / mMaxStepDown は、
//   「1ステップでどれくらいの段差・落差までスナップで許容するか」の目安。
//------------------------------------------------------------------------------
GravityComponent::GravityComponent(Actor* a)
    : Component(a)
    , mVelocityY(0.0f)
    , mGravityAccel(-80.0f)     // 重力加速度（ユニット/秒^2）
    , mJumpSpeed(35.0f)         // ジャンプ初速（ユニット/秒）
    , mMaxFallSpeed(-40.0f)     // 最大落下速度（負方向の終端速度）
    , mMaxStepUp(0.35f)         // 段差・上り坂として許容する最大高さ
    , mMaxStepDown(0.75f)       // 落下をスナップで拾う最大距離
    , mPenetrationEps(0.05f)    // めり込み許容量
    , mEnableGroundPose(false)
    , mIsGrounded(false)
{
    // ★ mPenetrationEps がメンバにあるならここで初期化しておくと安全。
    // mPenetrationEps = 0.05f;
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・1フレーム分の deltaTime を、最大 120fps 相当の小ステップに分割して処理する。
//   - 例：deltaTime が大きくても、内部では 1/120秒ごとの StepGravityOnce() を複数回呼ぶ。
//   - FPS が落ちた環境でも、重力・接地判定の精度を維持するためのサブステップ。
// ・実際の重力計算・接地スナップ処理は StepGravityOnce() に委譲。
//------------------------------------------------------------------------------
void GravityComponent::Update(float deltaTime)
{
    // 足コライダー必須（C_FOOT を持つ ColliderComponent）
    ColliderComponent* collider = FindFootCollider();
    if (!collider) return;

    float remaining = deltaTime;
    const float kMaxStep = 1.0f / 120.0f; // 重力・接地の内部ステップ（最大 120fps 相当）

    while (remaining > 0.0f)
    {
        float step = (remaining > kMaxStep) ? kMaxStep : remaining;
        StepGravityOnce(step, collider);
        remaining -= step;
    }
}

//------------------------------------------------------------------------------
// StepGravityOnce
//------------------------------------------------------------------------------
// ・重力・接地判定を「dt 分だけ」1ステップ進める処理。
// ・重力加速度から速度を積算し、終端速度でクランプしたあと、
//   足元の位置と groundY の関係から接地スナップを行う。
// ・PhysWorld::GetNearestGroundY() は、Actor の真下方向にある最も高い地面の Y を返す前提。
//   - Collider(C_GROUND) や地形メッシュから groundY を算出している想定。
//------------------------------------------------------------------------------
void GravityComponent::StepGravityOnce(float dt, ColliderComponent* collider)
{
    Actor* owner    = GetOwner();
    PhysWorld* phys = owner->GetApp()->GetPhysWorld();
    Vector3 pos     = owner->GetPosition();

    //--- 1. 加速度 → 速度（終端速度込み） ---
    mVelocityY += mGravityAccel * dt;

    // 落下速度に下限（終端速度）を設ける
    if (mVelocityY < mMaxFallSpeed)
    {
        mVelocityY = mMaxFallSpeed;
    }
    
    // 足元（C_FOOT の AABB の min.y）を基準に地面との関係を見る
    float footY     = collider->GetBoundingVolume()->GetWorldAABB().min.y;
    float nextFootY = footY + mVelocityY * dt;

    // 自分の真下にある「最も近い地面の Y」
    float groundY = -std::numeric_limits<float>::max();
    //bool hasGround = phys->GetNearestGroundY(owner, groundY);
    GroundHit hit;
    bool hasGround = phys->GetNearestGroundHit(owner, hit);
    groundY = hit.y;
    

    const float kMaxStepUp      = mMaxStepUp;      // 段差・上り坂として許せる上方向の差分
    const float kMaxStepDown    = mMaxStepDown;    // 落下をキャッチする下方向の差分
    const float kPenetrationEps = mPenetrationEps; // めり込み許容（浮かせ量との調整用）

    if (hasGround && mVelocityY <= 0.0f) // 下向きに動いているときだけ接地検査
    {
        // 次フレームの足元位置に対して、地面がどれくらい上 / 下にあるか
        //  yGapNext > 0 : 地面の方が高い（小さな段差・坂を登るケース）
        //  yGapNext < 0 : 地面の方が低い（落下して地面に着地するケース）
        float yGapNext = groundY - nextFootY;

        // 上り・下り両方を「スナップしてよい範囲」として許容する
        bool canSnap =
            (yGapNext >= -kMaxStepDown - kPenetrationEps) && // 下方向（落下キャッチ）
            (yGapNext <=  kMaxStepUp   + kPenetrationEps);   // 上方向（段差・坂）

        if (canSnap)
        {
            // groundY に足元が乗るように Actor の Y を補正
            float offset = pos.y - footY;           // Actor 原点 → 足元までのオフセット
            pos.y = groundY + offset + 0.001f;      // ごくわずかに浮かせて誤差によるめり込みを防ぐ
            owner->SetPosition(pos);

            mVelocityY  = 0.0f;                     // 着地したので速度リセット
            mIsGrounded = true;                     // 接地状態に遷移
            
            
            if (mIsGrounded && mEnableGroundPose)
            {
                ApplyGroundPose(owner, hit, dt);
            }
            
            return;
        }
    }

    // ここまで来たら今ステップでは接地しなかった扱い
    mIsGrounded = false;

    //--- 2. 通常の落下（位置の更新） ---
    pos.y += mVelocityY * dt;
    owner->SetPosition(pos);
}
/*void GravityComponent::ApplyGroundPose(Actor* owner, const GroundHit& hit)
{
    // ① ワールド normal → ローカル normal
    Quaternion inv = owner->GetRotation();
    inv.Conjugate();

    Vector3 localNormal = Vector3::Transform(hit.normal, inv);
    localNormal.Normalize();

    // ② ローカル Up と比較
    Vector3 localUp = Vector3::UnitY;

    float dot = Vector3::Dot(localUp, localNormal);
    dot = Math::Clamp(dot, -1.0f, 1.0f);

    float angle = Math::Acos(dot);
    if (angle < 0.001f)
    {
        owner->SetPoseRotation(Quaternion::Identity);
        return;
    }

    Vector3 axis = Vector3::Cross(localUp, localNormal);
    axis.Normalize();

    Quaternion pose(axis, angle);
    owner->SetPoseRotation(pose);
}
*/

void GravityComponent::ApplyGroundPose(Actor* owner, const GroundHit& hit, float dt)
{
    // ① ワールド normal → ローカル normal
    Quaternion inv = owner->GetRotation();
    inv.Conjugate();

    Vector3 localNormal = Vector3::Transform(hit.normal, inv);
    if (localNormal.LengthSq() < Math::NearZeroEpsilon)
    {
        return;
    }
    localNormal.Normalize();

    // ② ローカル Up と比較（ToyLibは左手・Y up前提でOK）
    const Vector3 localUp = Vector3::UnitY;

    float dot = Vector3::Dot(localUp, localNormal);
    dot = Math::Clamp(dot, -1.0f, 1.0f);

    float angle = Math::Acos(dot);

    // ③ 目標ポーズを作る（小さければ「戻る」を目標に）
    Quaternion targetPose = Quaternion::Identity;

    if (angle >= 0.001f)
    {
        Vector3 axis = Vector3::Cross(localUp, localNormal);

        // axis が極小（ほぼ平行/反平行）なら不安定なのでガード
        if (axis.LengthSq() > Math::NearZeroEpsilon)
        {
            axis.Normalize();
            targetPose = Quaternion(axis, angle);
        }
        // 反平行に近いケース（dot ≒ -1）を厳密にやるなら別処理が必要だけど、
        // 地面用途ならまずここまでで十分。
    }

    // ④ いまの Pose から targetPose へ Slerp
    //    「補間速度」は調整用。10〜20あたりから試すのが定番。
    const float poseLerpSpeed = 12.0f;
    float t = Math::Clamp(dt * poseLerpSpeed, 0.0f, 1.0f);

    Quaternion currentPose = owner->GetPoseRotation();
    Quaternion smoothPose  = Quaternion::Slerp(currentPose, targetPose, t);

    owner->SetPoseRotation(smoothPose);
}

//------------------------------------------------------------------------------
// Jump
//------------------------------------------------------------------------------
// ・接地中（mIsGrounded == true）のときだけジャンプ初速を与える。
// ・ジャンプ後は mIsGrounded を false にして空中状態に遷移。
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
// ・足用の Collider（カプセルや小さめ AABB）を 1 つ用意し、
//   それを地面判定専用として使用する前提のヘルパー関数。
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
