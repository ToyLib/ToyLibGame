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
//      * 上昇中   : 視点固定（ブレ防止）
//      * 落下中   : 見失いそうな時のみ追従
//      * 着地直後 : 自然に復帰（target 早め / camera 遅め）
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
    float GetYawSpeed() const
    {
        return mYawSpeed;
    }

    void SetYawSpeed(float speed)
    {
        mYawSpeed = speed;
    }

    //------------------------------------------------------------------
    // Air Y behavior control (delegates to CameraAirYController)
    //------------------------------------------------------------------
    void SetFreezeYInAir(bool enable)
    {
        mAirY.SetEnabled(enable);
    }

    bool GetFreezeYInAir() const
    {
        return mAirY.IsEnabled();
    }

    // 着地後の復帰速度（95% 到達秒）
    // 例: target=0.15, camera=0.30
    void SetRecoverSeconds(float targetSec, float cameraSec);

    // 落下中の救済追従速度（95% 到達秒）
    void SetFallAssistSeconds(float targetSec, float cameraSec);

    // 落下時の「見失い判定」閾値（ワールド Y 差）
    void SetFallOutOfViewThreshold(float thresholdY,
                                   float hysteresisY);

private:
    //==================================================================
    // Internal helpers
    //==================================================================
    void UpdateOrbit(float dt);
    void UpdateHeightAndDistance(float dt);

    Vector3 ComputeTarget() const;
    Vector3 ComputeIdealPos(const Vector3& target) const;

    void EnsureInitialPos(const Vector3& idealPos);
    void ApplyPositionLerp(const Vector3& idealPos, float dt);

    void ResolveGroundCollision(Vector3& ioCameraPos) const;
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

    //==================================================================
    // Air Y control (composed)
    //==================================================================
    CameraAirYController mAirY{};
};

} // namespace toy
