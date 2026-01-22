#include "Camera/FollowCameraComponent.h"

#include "Engine/Core/Actor.h"
#include "Physics/GravityComponent.h"

#include <cmath>

namespace {

//----------------------------------------------------------------------
// ExpApproach95
//  - seconds95 秒で 95% に近づく指数補間（バネらない）
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

FollowCameraComponent::FollowCameraComponent(Actor* owner)
    : CameraComponent(owner)
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

    // 空中Y制御の基準も同期
    mHoldCamY        = prevPos.y;
    mHoldTargetY     = prevTarget.y;
    mPrevGrounded    = true;
    mAirYMode        = AirYMode::None;
    mFallAssistActive = false;
}

//======================================================================
// Setters (Air-Y)
//======================================================================
void FollowCameraComponent::SetRecoverSeconds(float targetSec,
                                              float cameraSec)
{
    mRecoverTargetSeconds = (targetSec < 0.001f) ? 0.001f : targetSec;
    mRecoverCameraSeconds = (cameraSec < 0.001f) ? 0.001f : cameraSec;
}

void FollowCameraComponent::SetFallAssistSeconds(float targetSec,
                                                 float cameraSec)
{
    mFallAssistTargetSeconds = (targetSec < 0.001f) ? 0.001f : targetSec;
    mFallAssistCameraSeconds = (cameraSec < 0.001f) ? 0.001f : cameraSec;
}

void FollowCameraComponent::SetFallOutOfViewThreshold(float thresholdY,
                                                      float hysteresisY)
{
    mFallOutOfViewThresholdY  = (thresholdY < 0.0f) ? 0.0f : thresholdY;
    mFallOutOfViewHysteresisY = (hysteresisY < 0.0f) ? 0.0f : hysteresisY;
}

//======================================================================
// ProcessInput
//======================================================================
void FollowCameraComponent::ProcessInput(const InputState& state)
{
    float heightInput = 0.0f;

    // Orbit と同じ割当（S:上 / W:下）
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
    // 0) 高さ入力（Orbitと同じ：蓄積→Updateで消費）
    //--------------------------------------------------------------
    if (std::fabs(mHeightInput) > 1e-4f)
    {
        mVertDist += mHeightInput * mHeightSpeed * dt;
        mVertDist  = Math::Clamp(mVertDist, mMinVertDist, mMaxVertDist);
    }
    mHeightInput = 0.0f;

    //--------------------------------------------------------------
    // 1) 初回スナップ
    //--------------------------------------------------------------
    if (mFirstUpdate)
    {
        SnapToIdeal();
        mFirstUpdate = false;
    }

    //--------------------------------------------------------------
    // 2) 理想位置 → スプリング追従
    //--------------------------------------------------------------
    const Vector3 idealPos = ComputeCameraPos();
    UpdateSpring(mActualPos, mVelocity, idealPos, mSpring, dt);

    //--------------------------------------------------------------
    // 3) 注視点（Owner 前方）
    //--------------------------------------------------------------
    Vector3 cameraPos = mActualPos;
    Vector3 target    = ComputeTarget();

    //--------------------------------------------------------------
    // 4) 空中Y制御（camera/target を上書き）
    //--------------------------------------------------------------
    ApplyAirYControl(dt, cameraPos, target);

    //--------------------------------------------------------------
    // 5) View 行列反映
    //--------------------------------------------------------------
    Matrix4 view = Matrix4::CreateLookAt(cameraPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(cameraPos);

    mCameraTarget = target;

    // 次フレームに反映（スプリングの “結果” を保持）
    mActualPos = cameraPos;
}

//======================================================================
// SnapToIdeal
//======================================================================
void FollowCameraComponent::SnapToIdeal()
{
    mActualPos = ComputeCameraPos();
    mVelocity  = Vector3::Zero;

    Vector3 target = ComputeTarget();

    Matrix4 view = Matrix4::CreateLookAt(mActualPos, target, Vector3::UnitY);
    SetViewMatrix(view);
    SetCameraPosition(mActualPos);

    mCameraTarget = target;

    // 空中Y制御も同期
    mHoldCamY        = mActualPos.y;
    mHoldTargetY     = target.y;
    mPrevGrounded    = true;
    mAirYMode        = AirYMode::None;
    mFallAssistActive = false;
}

//======================================================================
// ComputeCameraPos
//======================================================================
Vector3 FollowCameraComponent::ComputeCameraPos() const
{
    Vector3 cameraPos = GetOwner()->GetPosition();
    cameraPos -= GetOwner()->GetForward() * mHorzDist;
    cameraPos += Vector3::UnitY * mVertDist;
    return cameraPos;
}

//======================================================================
// ComputeTarget
//======================================================================
Vector3 FollowCameraComponent::ComputeTarget() const
{
    Vector3 target =
        GetOwner()->GetPosition() +
        GetOwner()->GetForward() * mTargetDist;

    return target;
}

//======================================================================
// ApplyAirYControl
//======================================================================
void FollowCameraComponent::ApplyAirYControl(float dt,
                                             Vector3& ioCameraPos,
                                             Vector3& ioTarget)
{
    if (!mFreezeYInAir)
    {
        return;
    }

    auto* grav = GetOwner()->GetComponent<GravityComponent>();
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

    // ------------------------------------------------------------
    // Ground <-> Air transitions
    // ------------------------------------------------------------
    if (mPrevGrounded && inAir)
    {
        mHoldCamY    = ioCameraPos.y;
        mHoldTargetY = ioTarget.y;

        mAirYMode = AirYMode::Hold;
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

    // ------------------------------------------------------------
    // Apply mode
    // ------------------------------------------------------------
    switch (mAirYMode)
    {
    case AirYMode::Hold:
        ioCameraPos.y = mHoldCamY;
        ioTarget.y    = mHoldTargetY;
        break;

    case AirYMode::FallAssist:
    {
        const float drop = mHoldTargetY - desiredTargetY;

        if (!mFallAssistActive)
        {
            if (drop > mFallOutOfViewThresholdY)
            {
                mFallAssistActive = true;
            }
        }
        else
        {
            if (drop < (mFallOutOfViewThresholdY - mFallOutOfViewHysteresisY))
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

} // namespace toy
