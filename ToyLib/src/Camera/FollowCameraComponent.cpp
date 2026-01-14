#include "Camera/FollowCameraComponent.h"
#include "Engine/Core/Actor.h"

namespace toy {

FollowCameraComponent::FollowCameraComponent(Actor* owner)
    : CameraComponent(owner)
    , mHorzDist(10.0f)          // 所有 Actor から見た後方距離
    , mVertDist(4.0f)           // 高さオフセット
    , mTargetDist(10.0f)        // LookAt の前方オフセット
    , mSpring{ 200.0f, 1.0f }  // デフォルト：臨界減衰（振動なしで最速追従）
    , mActualPos(Vector3::Zero) // スプリングで補間される実際のカメラ位置
    , mVelocity(Vector3::Zero)  // スプリング内部用の速度
    , mFirstUpdate(true)        // 初回だけ理想位置にスナップするためのフラグ
{
}

//======================================================================
// OnActivated
//
//  ・他のカメラから Follow カメラに切り替わった瞬間に呼ばれる
//  ・prevPos    : 直前のカメラの位置
//    prevTarget : 直前のカメラの注視点
//
//  ・スプリングの開始位置を「前カメラの位置」に合わせることで、
//    視点がワープせず自然に Follow カメラの構図へ移行する
//======================================================================
void FollowCameraComponent::OnActivated(const Vector3& prevPos,
                                        const Vector3& prevTarget)
{
    // スプリングの初期位置を前カメラの位置に合わせる
    mActualPos = prevPos;
    mVelocity  = Vector3::Zero;

    // 基底クラスが保持する「現在位置/注視点」も同期
    mCameraPosition = prevPos;
    mCameraTarget   = prevTarget;

    // Manager 経由の切り替え時は、初回 SnapToIdeal は不要
    mFirstUpdate = false;
}

//======================================================================
// UpdateCamera
//
//  ・理想のカメラ位置を計算
//  ・スプリングを使ってその位置へ追従
//  ・所有 Actor の少し前方を注視点として View 行列を設定
//======================================================================
void FollowCameraComponent::UpdateCamera(float deltaTime)
{
    //------------------------------------------------------------------
    // 初回のみ、理想位置にワープしてガクつきを防ぐ
    //------------------------------------------------------------------
    if (mFirstUpdate)
    {
        SnapToIdeal();
        mFirstUpdate = false;
    }
    
    //------------------------------------------------------------------
    // 理想位置を算出 → スプリングで mActualPos を追従させる
    //------------------------------------------------------------------
    Vector3 idealPos = ComputeCameraPos();
    UpdateSpring(mActualPos, mVelocity, idealPos, mSpring, deltaTime);
    
    //------------------------------------------------------------------
    // 注視点：所有 Actor の前方（少し先）を狙う
    //------------------------------------------------------------------
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;
    
    //------------------------------------------------------------------
    // View 行列を更新し、Renderer に反映
    //------------------------------------------------------------------
    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);

    // CameraManager から参照される「現在の注視点」も更新
    mCameraTarget = target;
}

//======================================================================
// SnapToIdeal
//
//  ・スプリングを介さず、即座に理想位置へ配置する
//  ・テレポート直後や、初期配置のリセット時に使用
//======================================================================
void FollowCameraComponent::SnapToIdeal()
{
    // 理想位置へワープ
    mActualPos = ComputeCameraPos();
    mVelocity  = Vector3::Zero;
    
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;
    
    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);

    // 基底側のターゲットも同期（CameraManager 用）
    mCameraTarget = target;
}

//======================================================================
// ComputeCameraPos
//
//  ・所有 Actor の Transform から「理想的なカメラ位置」を算出
//      - 前方に対して mHorzDist だけ後ろに下がる
//      - 上方向に mVertDist だけ持ち上げる
//======================================================================
Vector3 FollowCameraComponent::ComputeCameraPos() const
{
    Vector3 cameraPos = GetOwner()->GetPosition();
    cameraPos -= GetOwner()->GetForward() * mHorzDist;
    cameraPos += Vector3::UnitY * mVertDist;
    return cameraPos;
}

} // namespace toy
