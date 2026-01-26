#include "Camera/CameraAirYController.h"

#include "Engine/Core/Actor.h"
#include "Physics/GravityComponent.h"

#include <cmath>

namespace {

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

void CameraAirYController::SetRecoverSeconds(float targetSec, float cameraSec)
{
    mRecoverTargetSeconds = (targetSec < 0.001f) ? 0.001f : targetSec;
    mRecoverCameraSeconds = (cameraSec < 0.001f) ? 0.001f : cameraSec;
}

void CameraAirYController::SetFallAssistSeconds(float targetSec, float cameraSec)
{
    mFallAssistTargetSeconds = (targetSec < 0.001f) ? 0.001f : targetSec;
    mFallAssistCameraSeconds = (cameraSec < 0.001f) ? 0.001f : cameraSec;
}

void CameraAirYController::SetFallOutOfViewThreshold(float thresholdY, float hysteresisY)
{
    mFallOutOfViewThresholdY  = (thresholdY < 0.0f) ? 0.0f : thresholdY;
    mFallOutOfViewHysteresisY = (hysteresisY < 0.0f) ? 0.0f : hysteresisY;
}

void CameraAirYController::Reset(const Actor* owner,
                                 const Vector3& cameraPos,
                                 const Vector3& target)
{
    (void)owner;

    mHoldCamY    = cameraPos.y;
    mHoldTargetY = target.y;

    mPrevGrounded     = true;
    mMode             = Mode::None;
    mFallAssistActive = false;
}

void CameraAirYController::Apply(const Actor* owner,
                                 float dt,
                                 Vector3& ioCameraPos,
                                 Vector3& ioTarget)
{
    if (!mEnabled)
    {
        return;
    }
    if (!owner)
    {
        return;
    }

    auto* grav = owner->GetComponent<GravityComponent>();
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

        mMode = Mode::Hold;
        mFallAssistActive = false;
    }
    else if (!mPrevGrounded && grounded)
    {
        mMode = Mode::Recover;
        mFallAssistActive = false;
    }

    if (inAir)
    {
        if (ascending)
        {
            mMode = Mode::Hold;
        }
        else if (falling)
        {
            if (mMode == Mode::Hold)
            {
                mMode = Mode::FallAssist;
            }
        }
        else
        {
            mMode = Mode::Hold;
        }
    }

    // ------------------------------------------------------------
    // Apply mode
    // ------------------------------------------------------------
    switch (mMode)
    {
        case Mode::Hold:
            ioCameraPos.y = mHoldCamY;
            ioTarget.y    = mHoldTargetY;
            break;

        case Mode::FallAssist:
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
                mHoldTargetY = ExpApproach95(mHoldTargetY,
                                             desiredTargetY,
                                             dt,
                                             mFallAssistTargetSeconds);

                mHoldCamY = ExpApproach95(mHoldCamY,
                                          desiredCamY,
                                          dt,
                                          mFallAssistCameraSeconds);
            }

            ioCameraPos.y = mHoldCamY;
            ioTarget.y    = mHoldTargetY;
            break;
        }

        case Mode::Recover:
        {
            mHoldTargetY = ExpApproach95(mHoldTargetY,
                                         desiredTargetY,
                                         dt,
                                         mRecoverTargetSeconds);

            mHoldCamY = ExpApproach95(mHoldCamY,
                                      desiredCamY,
                                      dt,
                                      mRecoverCameraSeconds);

            ioCameraPos.y = mHoldCamY;
            ioTarget.y    = mHoldTargetY;

            const bool doneTarget = (std::fabs(mHoldTargetY - desiredTargetY) < 0.01f);
            const bool doneCam    = (std::fabs(mHoldCamY - desiredCamY) < 0.01f);

            if (doneTarget && doneCam)
            {
                mMode = Mode::None;
            }
            break;
        }

        default:
            break;
    }

    mPrevGrounded = grounded;
}

} // namespace toy
