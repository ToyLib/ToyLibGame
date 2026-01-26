//======================================================================
// GravityComponent.h
//======================================================================
#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"
#include <cstdint>

namespace toy {
//======================================================================
// GravityComponent
//======================================================================
// 役割：
//  ・重力（Y 方向の加速度）を速度に積算し、Actor の上下移動を制御する
//  ・足元（C_FOOT Collider）の AABB(min.y) を基準に地面判定を行う
//  ・PhysWorld から取得した地面高さとの差分を使って
//      - 接地スナップ
//      - 落下
//      - 動く床への追従
//    を一貫して処理する
//
// 設計方針：
//  ・deltaTime は内部でサブステップに分割して処理
//    → 低 FPS 時でも床抜け・突き抜けを起こしにくい
//  ・地面情報（高さ / 法線 / 姿勢）は常にキャッシュ
//    → 見た目用（姿勢補正・影・エフェクト）に再利用できる
//  ・姿勢（PoseRotation）の反映はオプション（mEnableGroundPose）
//  ・上昇中は C_CEILING を検出して天井にめり込まないように制御
//======================================================================
class GravityComponent : public Component
{
public:
    explicit GravityComponent(Actor* owner);

    // 毎フレーム更新
    // 内部で deltaTime を小さなサブステップに分割して処理する
    void Update(float deltaTime) override;

    // 接地中のみジャンプ可能
    // 上向き初速を与えて空中状態に移行する
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

    // 段差の上り許容量（接地スナップ時）
    void  SetMaxStepUp(float v) { mMaxStepUp = v; }
    float GetMaxStepUp() const  { return mMaxStepUp; }

    // 段差の下り許容量（落下スナップ時）
    void  SetMaxStepDown(float v) { mMaxStepDown = v; }
    float GetMaxStepDown() const  { return mMaxStepDown; }

    // めり込み許容誤差
    void  SetPenetrationEps(float v) { mPenetrationEps = v; }
    float GetPenetrationEps() const  { return mPenetrationEps; }

    // 地面法線に合わせて Actor の姿勢を回転させるか（見た目用）
    void SetEnableGroundPose(bool b) { mEnableGroundPose = b; }
    bool GetEnableGroundPose() const { return mEnableGroundPose; }

    //--------------------------------------------------------------------------
    // 状態参照
    //--------------------------------------------------------------------------
    bool  IsGrounded()   const { return mIsGrounded; }
    float GetVelocityY() const { return mVelocityY; }

    // 現在乗っている床（Collider 床のみ）
    const class ColliderComponent* GetGroundCollider() const { return mGroundCollider; }

    //--------------------------------------------------------------------------
    // Ground pose cache（外部参照用）
    //--------------------------------------------------------------------------
    struct GroundPose
    {
        // valid：
        //  ・真下に地面情報が取れたかどうか
        //  ・空中でも「真下に床がある」なら true になり得る
        bool valid    = false;

        // grounded：
        //  ・スナップ接地が成立しているかどうか
        bool grounded = false;

        float   y      = 0.0f;               // 地面高さ
        Vector3 normal = Vector3::UnitY;     // ワールド法線

        // 地面に沿った姿勢
        //  raw    : 即応（影・UI 向け）
        //  smooth : 補間済み（キャラ見た目用）
        Quaternion raw    = Quaternion::Identity;
        Quaternion smooth = Quaternion::Identity;

        // Collider 床のときのみ有効
        const class ColliderComponent* collider = nullptr;
    };

    const GroundPose& GetGroundPose() const { return mGroundPose; }
    bool HasGroundPose() const { return mGroundPose.valid; }

    float   GetGroundY()      const { return mGroundPose.y; }
    Vector3 GetGroundNormal() const { return mGroundPose.normal; }
    
    bool HasFootBottom() const { return mHasFootBottom; }
    const Vector3& GetFootBottomPos() const { return mFootBottomPos; }

private:
    //--------------------------------------------------------------------------
    // 内部処理
    //--------------------------------------------------------------------------
    // サブステップ 1 回分の重力・接地処理
    void StepGravityOnce(float deltaTime, class ColliderComponent* footCollider);

    // 上昇中の天井押し戻し
    void ApplyCeilingClamp(Actor* owner);

    // 地面法線から姿勢を計算してキャッシュ更新
    // Actor への反映は行わない（呼び出し側で制御）
    void UpdateGroundPoseCache(Actor* owner,
                              const struct GroundHit& hit,
                              float deltaTime,
                              bool groundedNow,
                              bool wasGrounded);

    // C_FOOT を持つ Collider を取得
    ColliderComponent* FindFootCollider();

    // フレーム終端の保険：
    // 地面に深く潜っていた場合の最小補正
    void ApplyGroundDepenetration(Actor* owner, ColliderComponent* foot);
    
    static float GetOBBMinY(const struct OBB& obb);

private:
    //--------------------------------------------------------------------------
    // 内部状態
    //--------------------------------------------------------------------------
    uint32_t mSelfFlag{}; // 自分の種別（C_PLAYER 等）

    float mVelocityY{};
    float mGravityAccel{-60.0f};
    float mJumpSpeed{22.0f};
    float mMaxFallSpeed{-40.0f};

    bool  mIsGrounded{false};
    bool  mEnableGroundPose{false};

    // スナップ許容値
    float mMaxStepUp{0.35f};
    float mMaxStepDown{0.75f};
    float mPenetrationEps{};

    // 現在乗っている床
    const ColliderComponent* mGroundCollider{nullptr};
    Vector3 mPrevGroundPos{Vector3::Zero};

    // 地面情報キャッシュ
    GroundPose mGroundPose{};

    // 法線反転防止用
    Vector3 mPrevGroundNormal{Vector3::UnitY};
    
    bool    mHasFootBottom{false};
    Vector3 mFootBottomPos{Vector3::Zero};
};

} // namespace toy
