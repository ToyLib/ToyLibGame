#pragma once

#include "Camera/CameraComponent.h"
#include "Engine/Runtime/InputSystem.h" // InputState, GameButton

namespace toy {

//======================================================================
// SpringSettings
//======================================================================
struct SpringSettings
{
    float Stiffness    = 200.0f;   // バネ定数 k
    float DampingRatio = 1.0f;     // 減衰比 ζ
};

//======================================================================
// UpdateSpring
//======================================================================
inline void UpdateSpring(
    Vector3& position,
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
//  OrbitCamera と同じ方針：
//   - ProcessInput() で入力を蓄積
//   - UpdateCamera() で適用して 1フレームで消費
//   - 高さ操作は Y オフセット（mVertDist）だけ変更
//   - キー割当も Orbit と同じ（S:上 / W:下）
//======================================================================
class FollowCameraComponent : public CameraComponent
{
public:
    explicit FollowCameraComponent(Actor* owner);

    void ProcessInput(const InputState& state) override;
    void UpdateCamera(float deltaTime) override;

    void SnapToIdeal();

    void OnActivated(const Vector3& prevPos,
                     const Vector3& prevTarget) override;

    // パラメータ設定
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

    // 高さ制御（Orbitと合わせて調整しやすいように）
    void SetHeightRange(float minVert, float maxVert)
    {
        mMinVertDist = minVert;
        mMaxVertDist = maxVert;
    }

    void SetHeightSpeed(float speed)
    {
        mHeightSpeed = speed;
    }

private:
    Vector3 ComputeCameraPos() const;

private:
    // カメラの相対配置
    float mHorzDist{10.0f};   // 所有 Actor から見た後方距離
    float mVertDist{4.0f};   // 高さオフセット（これだけ W/S で変える）
    float mTargetDist{10.0f}; // LookAt の前方オフセット

    // スプリング設定
    SpringSettings mSpring{200.0f, 1.0f};

    // スプリングによって補間される実際のカメラ位置／速度
    Vector3 mActualPos{};
    Vector3 mVelocity{};

    // 初回のみ SnapToIdeal() するためのフラグ
    bool mFirstUpdate{true};

    //============================================================
    // Orbitと合わせる：入力は ProcessInput で蓄積 → Updateで消費
    //============================================================
    float mHeightInput{0.0f};   // 1フレーム分の高さ入力（-1/0/+1）
    float mHeightSpeed{7.0f};   // 高さ変化スピード（m/s）
    float mMinVertDist{1.0f};   // 最低高さ
    float mMaxVertDist{10.0f};   // 最高高さ
};

} // namespace toy
