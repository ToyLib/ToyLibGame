#include "Camera/FollowCameraComponent.h"

#include "Engine/Core/Actor.h"

namespace toy {

FollowCameraComponent::FollowCameraComponent(Actor* owner)
    : CameraComponent(owner)
{
    // デフォルトは無効（必要なカメラだけONにする運用が安全）
    mAirY.SetEnabled(false);
}

//======================================================================
// OnActivated
//======================================================================
void FollowCameraComponent::OnActivated(const Vector3& prevPos,
                                        const Vector3& prevTarget)
{
    mActualPos = prevPos;
    mVelocity  = Vector3::Zero;

    mCameraPosition = prevPos;
    mCameraTarget   = prevTarget;

    mFirstUpdate = false;

    // AirY の基準も同期（切替直後の急変を防ぐ）
    mAirY.Reset(GetOwner(), prevPos, prevTarget);
}

//======================================================================
// Air-Y setters (delegates)
//======================================================================
void FollowCameraComponent::SetRecoverSeconds(float targetSec,
                                              float cameraSec)
{
    mAirY.SetRecoverSeconds(targetSec, cameraSec);
}

void FollowCameraComponent::SetFallAssistSeconds(float targetSec,
                                                 float cameraSec)
{
    mAirY.SetFallAssistSeconds(targetSec, cameraSec);
}

void FollowCameraComponent::SetFallOutOfViewThreshold(float thresholdY,
                                                      float hysteresisY)
{
    mAirY.SetFallOutOfViewThreshold(thresholdY, hysteresisY);
}

//======================================================================
// ProcessInput
//
//  ・Orbit と同じ割当（S:上 / W:下）
//  ・ここでは蓄積だけして、UpdateCamera で 1フレーム消費する
//======================================================================
void FollowCameraComponent::ProcessInput(const InputState& state)
{
    float heightInput = 0.0f;

    if (state.IsButtonDown(GameButton::KeyS))
    {
        heightInput += 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyW))
    {
        heightInput -= 1.0f;
    }

    mHeightInput = heightInput;
}

//======================================================================
// UpdateCamera
//======================================================================
void FollowCameraComponent::UpdateCamera(float dt)
{
    //--------------------------------------------------------------
    // 0) 高さ入力を反映（Yオフセットのみ変更）
    //--------------------------------------------------------------
    if (Math::Abs(mHeightInput) > 1e-4f)
    {
        mVertDist += mHeightInput * mHeightSpeed * dt;
        mVertDist  = Math::Clamp(mVertDist, mMinVertDist, mMaxVertDist);
    }

    // 入力は 1 フレームで消費
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
    // 2) スプリング追従（理想位置へ）
    //--------------------------------------------------------------
    const Vector3 idealPos = ComputeIdealPos();
    UpdateSpring(mActualPos, mVelocity, idealPos, mSpring, dt);

    //--------------------------------------------------------------
    // 3) 注視点
    //--------------------------------------------------------------
    Vector3 cameraPos = mActualPos;
    Vector3 target    = ComputeTarget();

    //--------------------------------------------------------------
    // 4) 空中Y制御（camera / target の y を上書き）
    //--------------------------------------------------------------
    mAirY.Apply(GetOwner(), dt, cameraPos, target);

    //--------------------------------------------------------------
    // 5) View 行列反映
    //--------------------------------------------------------------
    Matrix4 view = Matrix4::CreateLookAt(cameraPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(cameraPos);

    mCameraTarget = target;

    //--------------------------------------------------------------
    // 6) スプリング状態へ反映
    //
    //  ・AirY によって cameraPos.y が変わるので、次フレームのスプリングが
    //    いきなり元のyへ戻ろうとしないよう、結果を保持する
    //--------------------------------------------------------------
    mActualPos = cameraPos;
}

//======================================================================
// SnapToIdeal
//======================================================================
void FollowCameraComponent::SnapToIdeal()
{
    mActualPos = ComputeIdealPos();
    mVelocity  = Vector3::Zero;

    Vector3 target = ComputeTarget();

    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);

    mCameraTarget = target;

    // AirY の基準も同期
    mAirY.Reset(GetOwner(), mActualPos, target);
}

//======================================================================
// ComputeIdealPos
//
//  ・Owner の後方へ mHorzDist
//  ・高さは mVertDist（W/S で変わる）
//======================================================================
Vector3 FollowCameraComponent::ComputeIdealPos() const
{
    Vector3 cameraPos = GetOwner()->GetPosition();

    cameraPos -= GetOwner()->GetForward() * mHorzDist;
    cameraPos += Vector3::UnitY * mVertDist;

    return cameraPos;
}

//======================================================================
// ComputeTarget
//
//  ・Owner の前方 mTargetDist を注視点とする
//======================================================================
Vector3 FollowCameraComponent::ComputeTarget() const
{
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;

    return target;
}

} // namespace toy
