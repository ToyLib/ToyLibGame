#pragma once

#include "Camera/CameraComponent.h"
#include "Camera/CameraAirYController.h"
#include "Engine/Runtime/InputSystem.h" // InputState, GameButton

namespace toy {

//======================================================================
// SpringSettings
//======================================================================
struct SpringSettings
{
    float Stiffness    = 200.0f; // バネ定数 k
    float DampingRatio = 1.0f;   // 減衰比 ζ（1.0 = 臨界減衰）
};

//======================================================================
// UpdateSpring
//
//  ・position を target に向けてスプリング追従させる
//  ・DampingRatio=1.0 のとき、臨界減衰（原理上オーバーシュートしにくい）
//======================================================================
inline void UpdateSpring(Vector3& position,
                         Vector3& velocity,
                         const Vector3& target,
                         const SpringSettings& settings,
                         float deltaTime)
{
    const float k = settings.Stiffness;
    const float z = settings.DampingRatio;

    // 減衰係数 c = 2 ζ √k
    const float c = 2.0f * z * Math::Sqrt(k);

    // x = current - target
    Vector3 diff = position - target;

    // a = -k x - c v
    Vector3 accel = -k * diff - c * velocity;

    velocity += accel * deltaTime;
    position += velocity * deltaTime;
}

//======================================================================
// FollowCameraComponent
//
//  ・所有 Actor を後方から追従する 3rd Person カメラ
//  ・高さ入力（W/S）は Orbit と同じ運用
//      - ProcessInput で蓄積
//      - UpdateCamera で適用して 1フレームで消費
//
//  ・空中Y制御（CameraAirYController）
//      - 上昇中   : camera.y / target.y を固定
//      - 落下中   : 見失いそうな時のみ追従
//      - 着地後   : target 早め / camera 遅めで自然復帰
//======================================================================
class FollowCameraComponent : public CameraComponent
{
public:
    explicit FollowCameraComponent(Actor* owner);

    //------------------------------------------------------------------
    // CameraComponent overrides
    //------------------------------------------------------------------
    void ProcessInput(const InputState& state) override;
    void UpdateCamera(float deltaTime) override;

    void OnActivated(const Vector3& prevPos,
                     const Vector3& prevTarget) override;

    //------------------------------------------------------------------
    // Helpers
    //------------------------------------------------------------------
    void SnapToIdeal();

    //------------------------------------------------------------------
    // Parameters
    //------------------------------------------------------------------
    void SetDistance(float horz, float vert)
    {
        mHorzDist = horz;
        mVertDist = vert;
    }

    void SetTargetDistance(float dist)
    {
        mTargetDist = dist;
    }

    void SetSpringSettings(const SpringSettings& s)
    {
        mSpring = s;
    }

    void SetHeightRange(float minVert, float maxVert)
    {
        mMinVertDist = minVert;
        mMaxVertDist = maxVert;
    }

    void SetHeightSpeed(float speed)
    {
        mHeightSpeed = speed;
    }

    //------------------------------------------------------------------
    // Air-Y behavior (delegates)
    //------------------------------------------------------------------
    void SetFreezeYInAir(bool enable)
    {
        mAirY.SetEnabled(enable);
    }

    bool GetFreezeYInAir() const
    {
        return mAirY.IsEnabled();
    }

    void SetRecoverSeconds(float targetSec, float cameraSec);
    void SetFallAssistSeconds(float targetSec, float cameraSec);
    void SetFallOutOfViewThreshold(float thresholdY,
                                   float hysteresisY);

private:
    Vector3 ComputeIdealPos() const;
    Vector3 ComputeTarget() const;

private:
    // ------------------------------------------------------------
    // Camera layout
    // ------------------------------------------------------------
    float mHorzDist{10.0f};
    float mVertDist{4.0f};
    float mTargetDist{10.0f};

    // ------------------------------------------------------------
    // Spring follow
    // ------------------------------------------------------------
    SpringSettings mSpring{200.0f, 1.0f};

    Vector3 mActualPos{Vector3::Zero};
    Vector3 mVelocity{Vector3::Zero};

    bool mFirstUpdate{true};

    // ------------------------------------------------------------
    // Input accumulation (Orbit style)
    // ------------------------------------------------------------
    float mHeightInput{0.0f};
    float mHeightSpeed{7.0f};
    float mMinVertDist{1.0f};
    float mMaxVertDist{10.0f};

    // ------------------------------------------------------------
    // Air Y controller (composed)
    // ------------------------------------------------------------
    CameraAirYController mAirY{};
};

} // namespace toy
