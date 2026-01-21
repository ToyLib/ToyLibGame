#include "Camera/FollowCameraComponent.h"

#include "Engine/Core/Actor.h"

#include <cmath>

namespace toy {

FollowCameraComponent::FollowCameraComponent(Actor* owner)
    : CameraComponent(owner)
    , mHorzDist(10.0f)          // 後方距離
    , mVertDist(4.0f)           // 高さ
    , mTargetDist(10.0f)        // LookAt 前方オフセット
    , mSpring{ 200.0f, 1.0f }   // 臨界減衰（振動なし）
    , mActualPos(Vector3::Zero)
    , mVelocity(Vector3::Zero)
    , mFirstUpdate(true)
    , mHeightInput(0.0f)
    , mHeightSpeed(7.0f)        // ★OrbitのheightSpeedと揃える
    , mMinVertDist(1.0f)
    , mMaxVertDist(10.0f)
{
}

//======================================================================
// OnActivated
//======================================================================
void FollowCameraComponent::OnActivated(const Vector3& prevPos,
                                        const Vector3& prevTarget)
{
    mActualPos      = prevPos;
    mVelocity       = Vector3::Zero;

    mCameraPosition = prevPos;
    mCameraTarget   = prevTarget;

    mFirstUpdate    = false;
}

//======================================================================
// ProcessInput
//
//  OrbitCamera と同じ流儀：
//   - ここでは値を「蓄積」するだけ
//   - 実際の反映は UpdateCamera 側
//   - キー割当も Orbit と同じ（S:上 / W:下）
//======================================================================
void FollowCameraComponent::ProcessInput(const InputState& state)
{
    float heightInput = 0.0f;

    // OrbitCamera と同じ割当（S:上 / W:下）
    if (state.IsButtonDown(GameButton::KeyS))
    {
        heightInput += 1.0f; // 上
    }
    if (state.IsButtonDown(GameButton::KeyW))
    {
        heightInput -= 1.0f; // 下
    }

    mHeightInput = heightInput;
}

//======================================================================
// UpdateCamera
//======================================================================
void FollowCameraComponent::UpdateCamera(float deltaTime)
{
    //--------------------------------------------------------------
    // 0) Orbit と同じ：高さ入力を適用（Yオフセットだけ変更）
    //--------------------------------------------------------------
    if (std::fabs(mHeightInput) > 1e-4f)
    {
        mVertDist += mHeightInput * mHeightSpeed * deltaTime;
        mVertDist  = Math::Clamp(mVertDist, mMinVertDist, mMaxVertDist);
    }

    // 入力は 1 フレームで消費（Orbitと同じ）
    mHeightInput = 0.0f;

    //--------------------------------------------------------------
    // 1) 初回のみ理想位置にスナップ
    //--------------------------------------------------------------
    if (mFirstUpdate)
    {
        SnapToIdeal();
        mFirstUpdate = false;
    }

    //--------------------------------------------------------------
    // 2) 理想位置 → スプリング追従
    //--------------------------------------------------------------
    Vector3 idealPos = ComputeCameraPos();
    UpdateSpring(mActualPos, mVelocity, idealPos, mSpring, deltaTime);

    //--------------------------------------------------------------
    // 3) 注視点：所有 Actor の前方
    //--------------------------------------------------------------
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;

    //--------------------------------------------------------------
    // 4) View 行列反映
    //--------------------------------------------------------------
    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);

    mCameraTarget = target;
}

//======================================================================
// SnapToIdeal
//======================================================================
void FollowCameraComponent::SnapToIdeal()
{
    mActualPos = ComputeCameraPos();
    mVelocity  = Vector3::Zero;

    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;

    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);

    mCameraTarget = target;
}

//======================================================================
// ComputeCameraPos
//======================================================================
Vector3 FollowCameraComponent::ComputeCameraPos() const
{
    Vector3 cameraPos = GetOwner()->GetPosition();
    cameraPos -= GetOwner()->GetForward() * mHorzDist;
    cameraPos += Vector3::UnitY * mVertDist; // ★ここが W/S で変わる
    return cameraPos;
}

} // namespace toy
