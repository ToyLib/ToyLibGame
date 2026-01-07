#pragma once

#include "Camera/CameraComponent.h"

namespace toy {

//======================================================================
// スプリング制御設定
//----------------------------------------------------------------------
// ・Stiffness（バネ定数 k）
// ・DampingRatio（減衰比 ζ）
//   ζ=1.0 で「臨界減衰」 → 振動せず最速で目標に追従
//======================================================================
struct SpringSettings
{
    float Stiffness    = 2000.0f;  // バネ定数 k
    float DampingRatio = 1.0f;     // 減衰比 ζ（推奨：1.0）
};

//======================================================================
// スプリング計算（位置・速度の更新）
//----------------------------------------------------------------------
// mass = 1 として、
//   a = -kx - cv
//   c = 2 ζ √k
// を使った標準的な 2nd Order Dynamics
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

    // a = -kx - c v
    Vector3 accel = -k * diff - c * velocity;

    velocity += accel * deltaTime;
    position += velocity * deltaTime;
}

//======================================================================
// FollowCameraComponent
//----------------------------------------------------------------------
// 所有 Actor の背後・上空にスプリングで追従する 3rd Person カメラ。
//  - スプリングによりスムーズ & ビクビクしない自然な動作を実現
//  - LookAt のターゲットは「所有 Actor の前方に少しオフセット」
//======================================================================
class FollowCameraComponent : public CameraComponent
{
public:
    FollowCameraComponent(Actor* owner);

    // deltaTime でスプリング追従し、ビュー行列を設定
    void UpdateCamera(float deltaTime) override;

    // 一瞬で理想位置にワープ（テレポート後などに使用）
    void SnapToIdeal();
    
    // カメラ切り替え時に呼ばれる
    void OnActivated(const Vector3& prevPos,
                     const Vector3& prevTarget) override;

    // パラメータ設定
    void SetDistance(float horz, float vert) { mHorzDist = horz; mVertDist = vert; }
    void SetTargetDistance(float dist)       { mTargetDist = dist; }
    void SetSpringSettings(const SpringSettings& s) { mSpring = s; }
    
private:
    // 所有アクターから見た「理想のカメラ位置」を計算
    Vector3 ComputeCameraPos() const;

private:
    // カメラ相対位置
    float mHorzDist;      // 後方距離
    float mVertDist;      // 高さオフセット
    float mTargetDist;    // 注視点の前方オフセット

    // スプリング構造体（Stiffness / Damping）
    SpringSettings mSpring;

    // 実際のカメラ位置（スプリングで補間していく）
    Vector3 mActualPos;
    Vector3 mVelocity;

    bool mFirstUpdate;
};

} // namespace toy
