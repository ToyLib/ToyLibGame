#include "Camera/OrbitCameraComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"
#include "Physics/GravityComponent.h"

#include <cmath>
#include <cfloat>

namespace {

//----------------------------------------------------------------------
// ExpApproach95
//
//  ・seconds95 秒で 95% まで近づく指数補間
//  ・オーバーシュートやバネ挙動が起きない
//----------------------------------------------------------------------
static float ExpApproach95(float current,
                           float target,
                           float dt,
                           float seconds95)
{
    const float T = (seconds95 < 0.001f) ? 0.001f : seconds95;
    const float k = 2.99573227355f / T; // ln(20)
    const float a = 1.0f - std::exp(-k * dt);

    return current + (target - current) * a;
}

} // anonymous namespace

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
}

//======================================================================
// Parameter setters
//======================================================================
void OrbitCameraComponent::SetRecoverSeconds(float targetSec,
                                             float cameraSec)
{
    mRecoverTargetSeconds = (targetSec < 0.001f) ? 0.001f : targetSec;
    mRecoverCameraSeconds = (cameraSec < 0.001f) ? 0.001f : cameraSec;
}

void OrbitCameraComponent::SetFallAssistSeconds(float targetSec,
                                                float cameraSec)
{
    mFallAssistTargetSeconds =
        (targetSec < 0.001f) ? 0.001f : targetSec;
    mFallAssistCameraSeconds =
        (cameraSec < 0.001f) ? 0.001f : cameraSec;
}

void OrbitCameraComponent::SetFallOutOfViewThreshold(float thresholdY,
                                                     float hysteresisY)
{
    mFallOutOfViewThresholdY =
        (thresholdY < 0.0f) ? 0.0f : thresholdY;
    mFallOutOfViewHysteresisY =
        (hysteresisY < 0.0f) ? 0.0f : hysteresisY;
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

    // 空中Y制御の基準値も同期
    mHoldCamY        = mCurrentPos.y;
    mHoldTargetY     = ComputeTarget().y;
    mPrevGrounded    = true;
    mAirYMode        = AirYMode::None;
    mFallAssistActive = false;

    mCameraPosition = prevPos;
    mCameraTarget   = ComputeTarget();
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
    ApplyAirYControl(dt, cameraPos, target);

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
        mOffset.y  = Math::Clamp(mOffset.y,
                                 mMinOffsetY,
                                 mMaxOffsetY);
    }

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

    mDistance = Math::Clamp(mDistance,
                            mMinDistance,
                            mMaxDistance);

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

        mHoldCamY     = mCurrentPos.y;
        mHoldTargetY  = ComputeTarget().y;
        mPrevGrounded = true;
        mAirYMode     = AirYMode::None;
        mFallAssistActive = false;
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

    mCurrentPos = Vector3::Lerp(mCurrentPos,
                                idealPos,
                                alpha);
}

//======================================================================
// Air Y control
//======================================================================
void OrbitCameraComponent::ApplyAirYControl(float dt,
                                            Vector3& ioCameraPos,
                                            Vector3& ioTarget)
{
    if (!mFreezeYInAir)
    {
        return;
    }

    auto* grav =
        GetOwner()->GetComponent<GravityComponent>();
    if (!grav)
    {
        return;
    }

    const bool grounded = grav->IsGrounded();
    const float velY    = grav->GetVelocityY();

    const bool inAir     = !grounded;
    const bool ascending = (velY > 0.0f);
    const bool falling   = (velY < 0.0f);

    const float desiredCamY    = ioCameraPos.y;
    const float desiredTargetY = ioTarget.y;

    // ---- state transitions
    if (mPrevGrounded && inAir)
    {
        mHoldCamY    = ioCameraPos.y;
        mHoldTargetY = ioTarget.y;
        mAirYMode    = AirYMode::Hold;
        mFallAssistActive = false;
    }
    else if (!mPrevGrounded && grounded)
    {
        mAirYMode = AirYMode::Recover;
        mFallAssistActive = false;
    }

    if (inAir)
    {
        if (ascending)
        {
            mAirYMode = AirYMode::Hold;
        }
        else if (falling)
        {
            if (mAirYMode == AirYMode::Hold)
            {
                mAirYMode = AirYMode::FallAssist;
            }
        }
        else
        {
            mAirYMode = AirYMode::Hold;
        }
    }

    // ---- apply mode
    switch (mAirYMode)
    {
    case AirYMode::Hold:
        ioCameraPos.y = mHoldCamY;
        ioTarget.y    = mHoldTargetY;
        break;

    case AirYMode::FallAssist:
    {
        const float drop =
            mHoldTargetY - desiredTargetY;

        if (!mFallAssistActive)
        {
            if (drop > mFallOutOfViewThresholdY)
            {
                mFallAssistActive = true;
            }
        }
        else
        {
            if (drop <
                (mFallOutOfViewThresholdY -
                 mFallOutOfViewHysteresisY))
            {
                mFallAssistActive = false;
            }
        }

        if (mFallAssistActive)
        {
            mHoldTargetY =
                ExpApproach95(mHoldTargetY,
                              desiredTargetY,
                              dt,
                              mFallAssistTargetSeconds);

            mHoldCamY =
                ExpApproach95(mHoldCamY,
                              desiredCamY,
                              dt,
                              mFallAssistCameraSeconds);
        }

        ioCameraPos.y = mHoldCamY;
        ioTarget.y    = mHoldTargetY;
        break;
    }

    case AirYMode::Recover:
    {
        mHoldTargetY =
            ExpApproach95(mHoldTargetY,
                          desiredTargetY,
                          dt,
                          mRecoverTargetSeconds);

        mHoldCamY =
            ExpApproach95(mHoldCamY,
                          desiredCamY,
                          dt,
                          mRecoverCameraSeconds);

        ioCameraPos.y = mHoldCamY;
        ioTarget.y    = mHoldTargetY;

        const bool doneTarget =
            (std::fabs(mHoldTargetY - desiredTargetY) < 0.01f);
        const bool doneCam =
            (std::fabs(mHoldCamY - desiredCamY) < 0.01f);

        if (doneTarget && doneCam)
        {
            mAirYMode = AirYMode::None;
        }
        break;
    }

    default:
        break;
    }

    mPrevGrounded = grounded;
}

//======================================================================
// Ground collision (camera only)
//======================================================================
void OrbitCameraComponent::ResolveGroundCollision(Vector3& ioCameraPos) const
{
    if (Application* app = GetOwner()->GetApp())
    {
        if (PhysWorld* phys = app->GetPhysWorld())
        {
            float groundY =
                phys->GetGroundHeightAt(ioCameraPos);

            if (groundY != -FLT_MAX)
            {
                const float margin = 0.1f;
                const float minY   = groundY + margin;

                if (ioCameraPos.y < minY)
                {
                    ioCameraPos.y = minY;
                }
            }
        }
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

    Matrix4 view =
        Matrix4::CreateLookAt(eye, at, up);

    SetViewMatrix(view);
}

} // namespace toy
