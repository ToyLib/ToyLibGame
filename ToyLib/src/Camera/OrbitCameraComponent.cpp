#include "Camera/OrbitCameraComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"

#include <cmath>
#include <cfloat>

namespace toy {

//======================================================================
// Constructor
//======================================================================
OrbitCameraComponent::OrbitCameraComponent(Actor* owner)
    : CameraComponent(owner)
{
    // 初期距離を mOffset から算出
    mDistance = mOffset.Length();
    mDistance = Math::Clamp(mDistance, mMinDistance, mMaxDistance);
    mTargetDistance = mDistance;

    // Y オフセットを制限
    mOffset.y = Math::Clamp(mOffset.y, mMinOffsetY, mMaxOffsetY);

    // デフォルトは無効（必要なカメラだけONにする運用が安全）
    mAirY.SetEnabled(false);
}

//======================================================================
// Parameter setters (delegates)
//======================================================================
void OrbitCameraComponent::SetRecoverSeconds(float targetSec,
                                             float cameraSec)
{
    mAirY.SetRecoverSeconds(targetSec, cameraSec);
}

void OrbitCameraComponent::SetFallAssistSeconds(float targetSec,
                                                float cameraSec)
{
    mAirY.SetFallAssistSeconds(targetSec, cameraSec);
}

void OrbitCameraComponent::SetFallOutOfViewThreshold(float thresholdY,
                                                     float hysteresisY)
{
    mAirY.SetFallOutOfViewThreshold(thresholdY, hysteresisY);
}

//======================================================================
// OnActivated
//======================================================================
void OrbitCameraComponent::OnActivated(const Vector3& prevPos,
                                       const Vector3& /*prevTarget*/)
{
    // 補間の開始点を前カメラ位置に合わせる
    mCurrentPos    = prevPos;
    mHasCurrentPos = true;

    Vector3 target = ComputeTarget();

    // AirY の基準も同期（切替直後に急変しないように）
    mAirY.Reset(GetOwner(), prevPos, target);

    mCameraPosition = prevPos;
    mCameraTarget   = target;
}

//======================================================================
// ProcessInput
//======================================================================
void OrbitCameraComponent::ProcessInput(const InputState& state)
{
    const float yawSpeedBase = Math::ToRadians(120.0f);

    float yawInput    = 0.0f;
    float heightInput = 0.0f;

    if (state.IsButtonDown(GameButton::KeyD))
    {
        yawInput += 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyA))
    {
        yawInput -= 1.0f;
    }

    // 既存仕様踏襲：S = 上 / W = 下
    if (state.IsButtonDown(GameButton::KeyS))
    {
        heightInput += 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyW))
    {
        heightInput -= 1.0f;
    }

    mYawSpeed    = yawInput * yawSpeedBase;
    mHeightInput = heightInput;
}

//======================================================================
// UpdateCamera (main loop)
//======================================================================
void OrbitCameraComponent::UpdateCamera(float dt)
{
    UpdateOrbit(dt);
    UpdateHeightAndDistance(dt);

    Vector3 target   = ComputeTarget();
    Vector3 idealPos = ComputeIdealPos(target);

    EnsureInitialPos(idealPos);
    ApplyPositionLerp(idealPos, dt);

    Vector3 cameraPos = mCurrentPos;

    // 空中Y制御（camera / target を上書き）
    mAirY.Apply(GetOwner(), dt, cameraPos, target);

    // 地面との当たり補正（カメラのみ）
    ResolveGroundCollision(cameraPos);

    mCurrentPos = cameraPos;

    ApplyView(cameraPos, target);
}

//======================================================================
// Orbit rotation
//======================================================================
void OrbitCameraComponent::UpdateOrbit(float dt)
{
    Quaternion yawRot(Vector3::UnitY, mYawSpeed * dt);

    mOffset   = Vector3::Transform(mOffset,   yawRot);
    mUpVector = Vector3::Transform(mUpVector, yawRot);
}

//======================================================================
// Height & distance control
//======================================================================
void OrbitCameraComponent::UpdateHeightAndDistance(float dt)
{
    const float heightSpeed = 7.0f;

    if (std::fabs(mHeightInput) > 1e-4f)
    {
        mOffset.y += mHeightInput * heightSpeed * dt;
        mOffset.y  = Math::Clamp(mOffset.y, mMinOffsetY, mMaxOffsetY);
    }

    // 入力は 1 フレームで消費
    mHeightInput = 0.0f;

    float t =
        (mOffset.y - mMinOffsetY) /
        (mMaxOffsetY - mMinOffsetY);

    t = Math::Clamp(t, 0.0f, 1.0f);

    mTargetDistance =
        mMinDistance +
        (mMaxDistance - mMinDistance) * t;

    const float zoomLerpSpeed = 10.0f;

    mDistance +=
        (mTargetDistance - mDistance) *
        zoomLerpSpeed * dt;

    mDistance = Math::Clamp(mDistance, mMinDistance, mMaxDistance);

    Vector3 dir = mOffset;
    if (!dir.IsZero())
    {
        dir.Normalize();
        mOffset = dir * mDistance;
    }
}

//======================================================================
// Target / Ideal position
//======================================================================
Vector3 OrbitCameraComponent::ComputeTarget() const
{
    return GetOwner()->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);
}

Vector3 OrbitCameraComponent::ComputeIdealPos(const Vector3& target) const
{
    return target + mOffset;
}

//======================================================================
// Initial position handling
//======================================================================
void OrbitCameraComponent::EnsureInitialPos(const Vector3& idealPos)
{
    if (!mHasCurrentPos)
    {
        mCurrentPos    = idealPos;
        mHasCurrentPos = true;

        Vector3 target = ComputeTarget();
        mAirY.Reset(GetOwner(), mCurrentPos, target);
    }
}

//======================================================================
// Position lerp
//======================================================================
void OrbitCameraComponent::ApplyPositionLerp(const Vector3& idealPos,
                                             float dt)
{
    float alpha = mPosLerpSpeed * dt;
    alpha = Math::Clamp(alpha, 0.0f, 1.0f);

    mCurrentPos = Vector3::Lerp(mCurrentPos, idealPos, alpha);
}

//======================================================================
// Ground collision (camera only)
//======================================================================
void OrbitCameraComponent::ResolveGroundCollision(Vector3& ioCameraPos) const
{
    Application* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }

    PhysWorld* phys = app->GetPhysWorld();
    if (!phys)
    {
        return;
    }

    float groundY = phys->GetGroundHeightAt(ioCameraPos);
    if (groundY == -FLT_MAX)
    {
        return;
    }

    const float margin = 0.1f;
    const float minY   = groundY + margin;

    if (ioCameraPos.y < minY)
    {
        ioCameraPos.y = minY;
    }
}

//======================================================================
// View matrix
//======================================================================
void OrbitCameraComponent::ApplyView(const Vector3& cameraPos,
                                     const Vector3& target)
{
    mCameraPosition = cameraPos;
    mCameraTarget   = target;

    Vector3 eye = cameraPos;
    Vector3 at  = target;
    Vector3 up  = mUpVector;

    Vector3 forward = at - eye;
    if (forward.IsZero())
    {
        forward = Vector3::UnitZ;
        at      = eye + forward;
    }

    forward.Normalize();

    float dotFU = Vector3::Dot(forward, up);
    if (std::fabs(dotFU) > 0.99f)
    {
        up = Vector3::UnitX;

        if (std::fabs(Vector3::Dot(forward, up)) > 0.99f)
        {
            up = Vector3::UnitZ;
        }
    }

    Matrix4 view = Matrix4::CreateLookAt(eye, at, up);
    SetViewMatrix(view);
}

} // namespace toy
