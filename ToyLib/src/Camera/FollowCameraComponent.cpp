#include "Camera/FollowCameraComponent.h"
#include "Engine/Core/Actor.h"

namespace toy {

FollowCameraComponent::FollowCameraComponent(Actor* owner)
    : CameraComponent(owner)
    , mHorzDist(10.0f)              // Actor の後方距離
    , mVertDist(4.0f)               // 高さオフセット
    , mTargetDist(10.0f)            // LookAt の前方オフセット
    , mSpring{ 2000.0f, 1.0f }      // デフォルト：臨界減衰（バネ制御）
    , mActualPos(Vector3::Zero)     // 現在のカメラ位置（スプリングで追従）
    , mVelocity(Vector3::Zero)      // スプリング内部速度
    , mFirstUpdate(true)            // 初回だけスナップ
{
    
}

void FollowCameraComponent::UpdateCamera(float deltaTime)
{
    
    //------------------------------------------------------------
    // 初回だけ理想位置にワープ（スプリング追従でのガクつきを防ぐ）
    //------------------------------------------------------------
    if (mFirstUpdate)
    {
        SnapToIdeal();
        mFirstUpdate = false;
    }
    
    //------------------------------------------------------------
    // 理想のカメラ位置を計算 → その位置へスプリングで追従
    //------------------------------------------------------------
    Vector3 idealPos = ComputeCameraPos();
    UpdateSpring(mActualPos, mVelocity, idealPos, mSpring, deltaTime);
    
    //------------------------------------------------------------
    // 注視点：Actor が向いている方向の少し前
    //------------------------------------------------------------
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;
    
    //------------------------------------------------------------
    // カメラ行列更新
    //------------------------------------------------------------
    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);
}

void FollowCameraComponent::SnapToIdeal()
{
    //------------------------------------------------------------
    // スプリングを使わず即座に理想位置へ移動
    //------------------------------------------------------------
    mActualPos = ComputeCameraPos();
    mVelocity  = Vector3::Zero;
    
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;
    
    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);
}

Vector3 FollowCameraComponent::ComputeCameraPos() const
{
    //------------------------------------------------------------
    // Actor の位置から、
    //   ・後方へ mHorzDist
    //   ・上へ mVertDist
    // に配置した位置が「理想位置」
    //------------------------------------------------------------
    Vector3 cameraPos = GetOwner()->GetPosition();
    cameraPos -= GetOwner()->GetForward() * mHorzDist;
    cameraPos += Vector3::UnitY * mVertDist;
    return cameraPos;
}

} // namespace toy
