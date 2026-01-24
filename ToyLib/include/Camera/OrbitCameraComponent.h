//======================================================================
// OrbitCameraComponent.h
//======================================================================
#pragma once

#include "Camera/CameraComponent.h"
#include "Camera/CameraAirYController.h"
#include "Utils/MathUtil.h"

namespace toy {

//======================================================================
// OrbitCameraComponent
//
//  ・ターゲット Actor の周囲を公転する 3rd Person カメラ
//  ・左手座標系（+Z が奥）前提
//  ・フィールドアクション / 見下ろし寄りのゲーム向け
//
//  主な特徴
//  ------------------------------------------------------------
//  - 左右入力で水平回転（Yaw）
//  - 上下入力で高さ変更（高さに応じて距離も自動調整）
//  - 空中時の Y 追従制御（CameraAirYController）
//
//  - 壁/遮蔽対策：
//    「target→camera のレイ」で遮蔽/衝突を検出し、
//    “上に逃がさず” target 側へ距離を詰める。
//    距離はスムーズに縮め、壁が無ければゆっくり戻す。
//======================================================================
class OrbitCameraComponent : public CameraComponent
{
public:
    explicit OrbitCameraComponent(class Actor* owner);

    //------------------------------------------------------------------
    // CameraComponent overrides
    //------------------------------------------------------------------
    void ProcessInput(const struct InputState& state) override;
    void UpdateCamera(float deltaTime) override;

    void OnActivated(const Vector3& prevPos,
                     const Vector3& prevTarget) override;

    //------------------------------------------------------------------
    // Orbit tuning
    //------------------------------------------------------------------
    float GetYawSpeed() const { return mYawSpeed; }
    void  SetYawSpeed(float speed) { mYawSpeed = speed; }

    //------------------------------------------------------------------
    // Air Y behavior control (delegates to CameraAirYController)
    //------------------------------------------------------------------
    void SetFreezeYInAir(bool enable) { mAirY.SetEnabled(enable); }
    bool GetFreezeYInAir() const { return mAirY.IsEnabled(); }

    void SetRecoverSeconds(float targetSec, float cameraSec);
    void SetFallAssistSeconds(float targetSec, float cameraSec);
    void SetFallOutOfViewThreshold(float thresholdY, float hysteresisY);

private:
    //==================================================================
    // Internal helpers
    //==================================================================
    void   UpdateOrbit(float dt);
    void   UpdateHeightAndDistance(float dt);

    Vector3 ComputeTarget() const;
    Vector3 ComputeIdealPos(const Vector3& target) const;

    void   EnsureInitialPos(const Vector3& idealPos);
    void   ApplyPositionLerp(const Vector3& idealPos, float dt);

    // 地面（Terrain/Collider床）補正：まず target 方向へ寄せ、それでもダメなら最小Y保険
    void ResolveGroundCollision(Vector3& ioCameraPos,
                                const Vector3& target,
                                float dt) const;

    // 遮蔽補正：target→camera のレイで壁があれば target 側へ寄せる（Y逃げ無し）
    void ResolveWallOcclusion(Vector3& ioCameraPos,
                              const Vector3& target,
                              float dt);

    // 壁衝突：target→desired のレイで「許可距離」を計算（状態は変えない）
    float ResolveWallCollisionLimit(const Vector3& target,
                                    const Vector3& desiredPos) const;

    // 壁衝突：許可距離に応じて mDistance をスムーズ更新
    void ApplyWallCollisionDistance(float dt,
                                    const Vector3& target,
                                    const Vector3& desiredPos);

    void ApplyView(const Vector3& cameraPos,
                   const Vector3& target);

private:
    //==================================================================
    // Orbit base parameters
    //==================================================================
    Vector3 mOffset{0.0f, 4.0f, -5.0f};
    Vector3 mUpVector{Vector3::UnitY};

    float mYawSpeed{0.0f};

    // Distance (zoom)
    float mDistance{0.0f};
    float mTargetDistance{0.0f};
    float mMinDistance{5.0f};
    float mMaxDistance{20.0f};

    // Height (offset Y)
    float mMinOffsetY{-2.0f};
    float mMaxOffsetY{8.0f};

    // Input accumulation
    float mHeightInput{0.0f};

    // Smooth movement
    Vector3 mCurrentPos{Vector3::Zero};
    bool    mHasCurrentPos{false};
    float   mPosLerpSpeed{8.0f};

    // 遮蔽で寄せる補間（強すぎると“飛び”やすいので 12 推奨）
    float   mWallLerpSpeed{12.0f};

    //==================================================================
    // Air Y control (composed)
    //==================================================================
    CameraAirYController mAirY{};

    //==================================================================
    // Wall collision tuning (distance control)
    //==================================================================
    float mCameraRadius{0.35f};          // 壁から離す余白（カメラ半径扱い）
    float mCollisionShrinkSpeed{20.0f};  // 壁がある時：縮む速度（速め）
    float mCollisionExpandSpeed{6.0f};   // 壁がない時：戻る速度（遅め）
    float mCollisionMinDistance{1.0f};   // 最低距離（詰まりすぎ防止）
};

} // namespace toy
