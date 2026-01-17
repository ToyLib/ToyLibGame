//======================================================================
// GravityComponent.h
//======================================================================
#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"
#include <cstdint>

namespace toy {

class Actor;
class ColliderComponent;
struct GroundHit;

// GroundHit 側に source がある前提（無ければ、この enum / 判定を削除してOK）
enum class GroundSource;

//------------------------------------------------------------------------------
// GravityComponent
//------------------------------------------------------------------------------
// ・重力（加速度）を Y 速度に積算し、Actor の上下移動を制御する。
// ・足元（C_FOOT Collider）の AABB(min.y) を基準に、PhysWorld の地面判定から
//   「次の足元位置」と「地面の高さ」の差を見て、スナップ接地／落下を処理する。
// ・deltaTime は内部で小ステップに分割して処理し、低FPSでも判定が破綻しにくい。
// ・地面情報（高さ/法線/姿勢Quaternion）は常にキャッシュし、外部から参照できる。
// ・Actor の PoseRotation 反映は必要なときだけ（mEnableGroundPose=true）。
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

    // Actor の PoseRotation を地面に合わせるか（見た目用）
    void SetEnableGroundPose(bool b) { mEnableGroundPose = b; }
    bool GetEnableGroundPose() const { return mEnableGroundPose; }

    //--------------------------------------------------------------------------
    // 状態参照
    //--------------------------------------------------------------------------
    bool  IsGrounded()   const { return mIsGrounded; }
    float GetVelocityY() const { return mVelocityY; }

    const ColliderComponent* GetGroundCollider() const { return mGroundCollider; }

    //--------------------------------------------------------------------------
    // Ground pose cache（外部参照用）
    //--------------------------------------------------------------------------
    struct GroundPose
    {
        // ★重要：
        // valid は「真下に地面情報が取れたか」を表す。
        // ジャンプ中など空中でも、真下に地面が取れているなら true になり得る。
        bool valid    = false;
        bool grounded = false;               // 接地中か（スナップ成立）

        float   y      = 0.0f;               // ground height
        Vector3 normal = Vector3::UnitY;     // world normal

        // “地面に沿う姿勢”
        // raw    : 即応（遅れ無し。影/リング向け）
        // smooth : 補間済み（車/4本足など見た目用）
        Quaternion raw    = Quaternion::Identity;
        Quaternion smooth = Quaternion::Identity;

        const ColliderComponent* collider = nullptr; // Collider床のときだけ
        // GroundSource source = GroundSource::None; // 必要なら
    };

    const GroundPose& GetGroundPose() const { return mGroundPose; }
    bool HasGroundPose() const { return mGroundPose.valid; }

private:
    // サブステップ1回分の重力・接地処理
    void StepGravityOnce(float deltaTime, ColliderComponent* footCollider);

    // 上昇中の天井押し戻し
    void ApplyCeilingClamp(Actor* owner);

    // 地面法線に合わせた姿勢を計算してキャッシュを更新（※Actorへは反映しない）
    // groundedNow / wasGrounded を明示し、接地開始時の跳ねを抑える
    void UpdateGroundPoseCache(Actor* owner,
                              const GroundHit& hit,
                              float deltaTime,
                              bool groundedNow,
                              bool wasGrounded);

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

    // 今乗ってる床（Collider床）
    const ColliderComponent* mGroundCollider = nullptr;
    Vector3 mPrevGroundPos = Vector3::Zero;

    // 地面情報キャッシュ
    GroundPose mGroundPose;
};

} // namespace toy
