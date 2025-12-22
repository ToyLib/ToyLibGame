#pragma once

#include "Engine/Core/Component.h"
#include <cstdint>

namespace toy {

class Actor;
class ColliderComponent;
struct GroundHit;

//------------------------------------------------------------------------------
// GravityComponent
//------------------------------------------------------------------------------
// ・重力（加速度）を Y 速度に積算し、Actor の上下移動を制御する。
// ・足元（C_FOOT Collider）の AABB(min.y) を基準に、PhysWorld の地面判定から
//   「次の足元位置」と「地面の高さ」の差を見て、スナップ接地／落下を処理する。
// ・deltaTime は内部で小ステップに分割して処理し、低FPSでも判定が破綻しにくい。
// ・必要なら地面法線に合わせてポーズ（PoseRotation）を滑らかに補正する。
// ・上昇中は天井（C_CEILING）を検出して押し戻し、上向き速度をクランプする。
//------------------------------------------------------------------------------
class GravityComponent : public Component
{
public:
    explicit GravityComponent(Actor* owner);

    // 毎フレーム更新（内部でサブステップ化）
    void Update(float deltaTime) override;

    // 接地中のみジャンプ（上向き初速を与えて空中へ）
    void Jump();

    //--------------------------------------------------------------------------
    // フラグ設定
    //--------------------------------------------------------------------------
    // 自分自身のコライダー種別（例：C_PLAYER / C_ENEMY）
    void     SetSelfFlag(uint32_t flag) { mSelfFlag = flag; }
    uint32_t GetSelfFlag() const        { return mSelfFlag; }

    //--------------------------------------------------------------------------
    // 調整用パラメータ
    //--------------------------------------------------------------------------
    void  SetGravityAccel(float g) { mGravityAccel = g; }
    float GetGravityAccel() const  { return mGravityAccel; }

    void  SetJumpSpeed(float s) { mJumpSpeed = s; }
    float GetJumpSpeed() const  { return mJumpSpeed; }

    void  SetMaxFallSpeed(float v) { mMaxFallSpeed = v; }
    float GetMaxFallSpeed() const  { return mMaxFallSpeed; }

    void  SetMaxStepUp(float v) { mMaxStepUp = v; }
    float GetMaxStepUp() const  { return mMaxStepUp; }

    void  SetMaxStepDown(float v) { mMaxStepDown = v; }
    float GetMaxStepDown() const  { return mMaxStepDown; }

    void  SetPenetrationEps(float v) { mPenetrationEps = v; }
    float GetPenetrationEps() const  { return mPenetrationEps; }

    void SetEnableGroundPose(bool b) { mEnableGroundPose = b; }
    bool GetEnableGroundPose() const { return mEnableGroundPose; }

    //--------------------------------------------------------------------------
    // 状態参照
    //--------------------------------------------------------------------------
    bool  IsGrounded() const { return mIsGrounded; }
    float GetVelocityY() const { return mVelocityY; }
    
    const ColliderComponent* GetGroundCollider() const { return mGroundCollider; }

private:
    // サブステップ1回分の重力・接地処理
    void StepGravityOnce(float deltaTime, ColliderComponent* footCollider);

    // 上昇中の天井押し戻し
    void ApplyCeilingClamp(Actor* owner);

    // 地面法線に合わせたポーズ補正
    void ApplyGroundPose(Actor* owner, const GroundHit& hit, float deltaTime);

    // C_FOOT を持つ ColliderComponent を探す
    ColliderComponent* FindFootCollider();
    
    // フレーム終端の保険：
    // 「地面に潜っていたら」最小限だけ引き上げて整合を取る
    void ApplyGroundDepenetration(Actor* owner, ColliderComponent* foot);

private:
    //--------------------------------------------------------------------------
    // フラグ
    //--------------------------------------------------------------------------
    uint32_t mSelfFlag = 0; // 自分の種別（C_PLAYER など）

    //--------------------------------------------------------------------------
    // 速度・状態
    //--------------------------------------------------------------------------
    float mVelocityY    = 0.0f;
    float mGravityAccel = -80.0f;
    float mJumpSpeed    = 35.0f;
    float mMaxFallSpeed = -40.0f;

    bool  mIsGrounded       = false;
    bool  mEnableGroundPose = false;

    //--------------------------------------------------------------------------
    // スナップ許容パラメータ
    //--------------------------------------------------------------------------
    float mMaxStepUp      = 0.35f;
    float mMaxStepDown    = 0.75f;
    float mPenetrationEps = 0.05f;
    
    const ColliderComponent* mGroundCollider = nullptr;  // 今乗ってる C_GROUND
    Vector3 mPrevGroundPos = Vector3::Zero;        // 前回の床位置（ワールド）
};

} // namespace toy
