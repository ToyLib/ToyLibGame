//======================================================================
// OrbitCameraComponent.cpp
//======================================================================
#include "Camera/OrbitCameraComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"

#include <cmath>
#include <cfloat>
#include <algorithm>

namespace toy {

namespace
{
//======================================================================
// FrameRateIndependentAlpha
//
// 従来の
//
//     alpha = speed * dt;
//
// という補間を、FPSに依存しない減衰へ変換する。
//
// 60FPS時のフィーリングを基準として維持する。
// 例:
//     speed = 10
//     60FPS時 alpha = 10 / 60 = 0.166666...
//
// 30 / 60 / 120 / 240 FPSでも、
// 同じ実時間でほぼ同じ追従量になる。
//======================================================================
float FrameRateIndependentAlpha(float speed, float deltaTime)
{
    constexpr float referenceFPS = 60.0f;

    if (speed <= 0.0f || deltaTime <= 0.0f)
    {
        return 0.0f;
    }

    const float baseAlpha =
        Math::Clamp(speed / referenceFPS, 0.0f, 1.0f);

    // baseAlpha == 1 のとき pow(0, ...) を避けて即追従
    if (baseAlpha >= 1.0f)
    {
        return 1.0f;
    }

    const float alpha =
        1.0f -
        std::pow(
            1.0f - baseAlpha,
            deltaTime * referenceFPS
        );

    return Math::Clamp(alpha, 0.0f, 1.0f);
}

} // namespace


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
    // rad/sec
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
    //--------------------------------------------------------------
    // 0) 入力による公転・高さ変更
    //--------------------------------------------------------------
    UpdateOrbit(dt);
    UpdateHeightAndDistance(dt);

    //--------------------------------------------------------------
    // 1) 注視点
    //--------------------------------------------------------------
    Vector3 target = ComputeTarget();

    //--------------------------------------------------------------
    // 2) 現在距離を前提とした理想位置
    //--------------------------------------------------------------
    Vector3 idealPos = ComputeIdealPos(target);

    //--------------------------------------------------------------
    // 3) 壁衝突
    //
    // mDistance を
    //
    //  ・壁あり → allowedDist へ素早く縮める
    //  ・壁なし → mTargetDistance へ戻す
    //
    // 距離追従はここで一元管理する。
    //--------------------------------------------------------------
    ApplyWallCollisionDistance(dt, target, idealPos);

    //--------------------------------------------------------------
    // 4) mDistance が変化したので理想位置を再計算
    //--------------------------------------------------------------
    idealPos = ComputeIdealPos(target);

    //--------------------------------------------------------------
    // 5) カメラ位置追従
    //--------------------------------------------------------------
    EnsureInitialPos(idealPos);
    ApplyPositionLerp(idealPos, dt);

    Vector3 cameraPos = mCurrentPos;

    //--------------------------------------------------------------
    // 6) 空中Y制御
    //--------------------------------------------------------------
    mAirY.Apply(GetOwner(), dt, cameraPos, target);

    //--------------------------------------------------------------
    // 7) 壁による遮蔽回避
    //--------------------------------------------------------------
    ResolveWallOcclusion(cameraPos, target, dt);

    //--------------------------------------------------------------
    // 8) 地面衝突補正
    //--------------------------------------------------------------
    ResolveGroundCollision(cameraPos, target, dt);

    //--------------------------------------------------------------
    // 9) 最終状態
    //--------------------------------------------------------------
    mCurrentPos = cameraPos;

    ApplyView(cameraPos, target);
}


//======================================================================
// Orbit rotation
//
// mYawSpeed : rad/sec
//======================================================================
void OrbitCameraComponent::UpdateOrbit(float dt)
{
    if (Math::Abs(mYawSpeed) <= Math::NearZeroEpsilon)
    {
        return;
    }

    const Quaternion yawRot(
        Vector3::UnitY,
        mYawSpeed * dt
    );

    mOffset =
        Vector3::Transform(mOffset, yawRot);

    mUpVector =
        Vector3::Transform(mUpVector, yawRot);
}


//======================================================================
// Height & distance control
//
// ここでは
//
//  ・高さ変更
//  ・高さに応じた「目標距離 mTargetDistance」の算出
//
// のみを行う。
//
// 実際の mDistance の追従は ApplyWallCollisionDistance() に
// 一元化する。
//======================================================================
void OrbitCameraComponent::UpdateHeightAndDistance(float dt)
{
    constexpr float heightSpeed = 7.0f;

    //--------------------------------------------------------------
    // 高さ操作
    //--------------------------------------------------------------
    if (std::fabs(mHeightInput) > 1e-4f)
    {
        mOffset.y +=
            mHeightInput *
            heightSpeed *
            dt;

        mOffset.y =
            Math::Clamp(
                mOffset.y,
                mMinOffsetY,
                mMaxOffsetY
            );
    }

    // 入力は1フレームで消費
    mHeightInput = 0.0f;

    //--------------------------------------------------------------
    // 高さ → 理想距離
    //--------------------------------------------------------------
    const float range =
        mMaxOffsetY - mMinOffsetY;

    float t = 0.0f;

    if (Math::Abs(range) > Math::NearZeroEpsilon)
    {
        t =
            (mOffset.y - mMinOffsetY) /
            range;

        t = Math::Clamp(t, 0.0f, 1.0f);
    }

    mTargetDistance =
        mMinDistance +
        (mMaxDistance - mMinDistance) * t;

    //--------------------------------------------------------------
    // 高さ変更で mOffset の方向が変わっている可能性があるので、
    // 現在の mDistance を維持したまま方向だけ反映する。
    //--------------------------------------------------------------
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
    return
        GetOwner()->GetPosition() +
        Vector3(0.0f, 2.5f, 0.0f);
}

Vector3 OrbitCameraComponent::ComputeIdealPos(
    const Vector3& target) const
{
    return target + mOffset;
}


//======================================================================
// Initial position handling
//======================================================================
void OrbitCameraComponent::EnsureInitialPos(
    const Vector3& idealPos)
{
    if (!mHasCurrentPos)
    {
        mCurrentPos    = idealPos;
        mHasCurrentPos = true;

        Vector3 target = ComputeTarget();

        mAirY.Reset(
            GetOwner(),
            mCurrentPos,
            target
        );
    }
}


//======================================================================
// Position lerp
//
// 60FPS時の既存フィーリングを維持しながら
// FPS非依存の補間にする。
//======================================================================
void OrbitCameraComponent::ApplyPositionLerp(
    const Vector3& idealPos,
    float dt)
{
    const float alpha =
        FrameRateIndependentAlpha(
            mPosLerpSpeed,
            dt
        );

    mCurrentPos =
        Vector3::Lerp(
            mCurrentPos,
            idealPos,
            alpha
        );
}


//======================================================================
// Ground collision (camera only)
//
//  ・地面に潜りそうなとき「上に逃がす」と被写体を見失いやすい。
//  ・まず target 方向へ寄せる。
//  ・それでも潜る場合のみ最小限のY補正を行う。
//======================================================================
void OrbitCameraComponent::ResolveGroundCollision(
    Vector3& ioCameraPos,
    const Vector3& target,
    float dt) const
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

    //==========================================================
    // (1) カメラ直下だけを見る
    //==========================================================
    constexpr float kStartUp = 0.05f;
    constexpr float kRayDown = 50.0f;
    constexpr float kAllowUp = 0.10f;
    constexpr float kMargin  = 0.10f;

    GroundHit hit;

    const Vector3 origin(
        ioCameraPos.x,
        ioCameraPos.y + kStartUp,
        ioCameraPos.z
    );

    if (!phys->GetGroundHitRayDown(
            origin,
            kRayDown,
            C_GROUND,
            hit) ||
        !hit.hit)
    {
        return;
    }

    //==========================================================
    // (2) カメラより上の地面を無視
    //==========================================================
    if (hit.y > ioCameraPos.y + kAllowUp)
    {
        return;
    }

    const float minY = hit.y + kMargin;

    if (ioCameraPos.y >= minY)
    {
        return;
    }

    //==========================================================
    // (3) まずtarget方向へ寄せる
    //==========================================================
    Vector3 toTarget =
        target - ioCameraPos;

    const float distToTarget =
        toTarget.Length();

    if (distToTarget > Math::NearZeroEpsilon)
    {
        toTarget *=
            1.0f / distToTarget;

        const float dy = toTarget.y;

        if (std::fabs(dy) > 1e-6f)
        {
            float s =
                (minY - ioCameraPos.y) /
                dy;

            if (s > 0.0f)
            {
                const float maxSlide =
                    std::max(
                        0.0f,
                        distToTarget - 0.05f
                    );

                s =
                    Math::Clamp(
                        s,
                        0.0f,
                        maxSlide
                    );

                ioCameraPos +=
                    toTarget * s;
            }
        }
    }

    //==========================================================
    // (4) 最後の保険：最低Yまで補正
    //==========================================================
    if (ioCameraPos.y < minY)
    {
        constexpr float kGroundLerpSpeed = 18.0f;

        const float alpha =
            FrameRateIndependentAlpha(
                kGroundLerpSpeed,
                dt
            );

        ioCameraPos.y =
            ioCameraPos.y +
            (minY - ioCameraPos.y) *
            alpha;
    }
}


//======================================================================
// Wall occlusion
//
// target -> camera の間に壁がある場合、
// 上へ逃げずにtarget側へカメラを寄せる。
//======================================================================
void OrbitCameraComponent::ResolveWallOcclusion(
    Vector3& ioCameraPos,
    const Vector3& target,
    float dt)
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

    Vector3 camDir =
        ioCameraPos - target;

    const float camDist =
        camDir.Length();

    if (camDist <= Math::NearZeroEpsilon)
    {
        return;
    }

    camDir *=
        1.0f / camDist;

    RaycastHit hit{};

    if (!phys->Raycast(
            target,
            camDir,
            camDist,
            C_WALL,
            hit))
    {
        return;
    }

    //==========================================================
    // (A) 線分範囲外ヒットを無視
    //==========================================================
    if (!(hit.distance > 0.0f &&
          hit.distance <= camDist))
    {
        return;
    }

    //==========================================================
    // (A2) 開始点付近の誤ヒットを無視
    //==========================================================
    const float kMinValidHitDist =
        std::max(
            0.05f,
            mCameraRadius * 0.25f
        );

    if (hit.distance < kMinValidHitDist)
    {
        return;
    }

    //==========================================================
    // (B) 壁らしい面だけ採用
    //==========================================================
    constexpr float kWallNormalYMax = 0.35f;

    if (std::fabs(hit.normal.y) > kWallNormalYMax)
    {
        return;
    }

    //==========================================================
    // (C) 上空ヒットを無視
    //==========================================================
    const Vector3 hitPos =
        target +
        camDir * hit.distance;

    const float yMin =
        std::min(
            target.y,
            ioCameraPos.y
        );

    const float yMax =
        std::max(
            target.y,
            ioCameraPos.y
        );

    const float yPad =
        mCameraRadius + 0.25f;

    if (hitPos.y < yMin - yPad ||
        hitPos.y > yMax + yPad)
    {
        return;
    }

    //==========================================================
    // 壁の手前へ寄せる
    //==========================================================
    const float backOff =
        mCameraRadius + 0.05f;

    float allowedDist =
        hit.distance - backOff;

    allowedDist =
        Math::Clamp(
            allowedDist,
            mCollisionMinDistance,
            camDist
        );

    const Vector3 desiredPos =
        target +
        camDir * allowedDist;

    const float alpha =
        FrameRateIndependentAlpha(
            mWallLerpSpeed,
            dt
        );

    ioCameraPos =
        Vector3::Lerp(
            ioCameraPos,
            desiredPos,
            alpha
        );
}


//======================================================================
// Wall collision limit (pure calculation)
//======================================================================
float OrbitCameraComponent::ResolveWallCollisionLimit(
    const Vector3& target,
    const Vector3& desiredPos) const
{
    Application* app = GetOwner()->GetApp();
    if (!app)
    {
        return -1.0f;
    }

    PhysWorld* phys = app->GetPhysWorld();
    if (!phys)
    {
        return -1.0f;
    }

    Vector3 dir =
        desiredPos - target;

    const float dist =
        dir.Length();

    if (dist <= Math::NearZeroEpsilon)
    {
        return -1.0f;
    }

    dir.Normalize();

    RaycastHit hit{};

    if (!phys->Raycast(
            target,
            dir,
            dist,
            C_WALL,
            hit))
    {
        return -1.0f;
    }

    //==========================================================
    // (A) 線分範囲外ヒットを無視
    //==========================================================
    if (!(hit.distance > 0.0f &&
          hit.distance <= dist))
    {
        return -1.0f;
    }

    //==========================================================
    // (A2) 開始点付近の誤ヒットを無視
    //==========================================================
    const float kMinValidHitDist =
        std::max(
            0.05f,
            mCameraRadius * 0.25f
        );

    if (hit.distance < kMinValidHitDist)
    {
        return -1.0f;
    }

    //==========================================================
    // (B) 壁らしい面だけ採用
    //==========================================================
    constexpr float kWallNormalYMax = 0.35f;

    if (std::fabs(hit.normal.y) > kWallNormalYMax)
    {
        return -1.0f;
    }

    //==========================================================
    // (C) 上空ヒットを無視
    //==========================================================
    const Vector3 hitPos =
        target +
        dir * hit.distance;

    const float yMin =
        std::min(
            target.y,
            desiredPos.y
        );

    const float yMax =
        std::max(
            target.y,
            desiredPos.y
        );

    const float yPad =
        mCameraRadius + 0.25f;

    if (hitPos.y < yMin - yPad ||
        hitPos.y > yMax + yPad)
    {
        return -1.0f;
    }

    float allowed =
        hit.distance -
        mCameraRadius;

    allowed =
        std::max(
            allowed,
            mCollisionMinDistance
        );

    return allowed;
}


//======================================================================
// Apply wall collision distance
//
// 距離制御はここへ一元化。
//
// 壁なし:
//     mTargetDistanceへ戻す
//
// 壁あり:
//     allowedDistへ縮める
//
// 補間係数は60FPS時の既存フィーリングを基準にFPS非依存化。
//======================================================================
void OrbitCameraComponent::ApplyWallCollisionDistance(
    float dt,
    const Vector3& target,
    const Vector3& desiredPos)
{
    const float allowedDist =
        ResolveWallCollisionLimit(
            target,
            desiredPos
        );

    if (allowedDist <= 0.0f)
    {
        //----------------------------------------------------------
        // 制限なし
        // → 理想距離へ戻す
        //----------------------------------------------------------
        const float alpha =
            FrameRateIndependentAlpha(
                mCollisionExpandSpeed,
                dt
            );

        mDistance +=
            (mTargetDistance - mDistance) *
            alpha;
    }
    else
    {
        //----------------------------------------------------------
        // 壁あり
        // → 安全距離へ縮める
        //----------------------------------------------------------
        const float alpha =
            FrameRateIndependentAlpha(
                mCollisionShrinkSpeed,
                dt
            );

        mDistance +=
            (allowedDist - mDistance) *
            alpha;
    }

    mDistance =
        Math::Clamp(
            mDistance,
            mMinDistance,
            mMaxDistance
        );

    //--------------------------------------------------------------
    // mOffsetの方向を維持して距離だけ更新
    //--------------------------------------------------------------
    Vector3 dir = mOffset;

    if (dir.IsZero())
    {
        dir =
            Vector3(
                0.0f,
                0.0f,
                -1.0f
            );
    }

    dir.Normalize();

    mOffset =
        dir * mDistance;
}


//======================================================================
// View matrix
//======================================================================
void OrbitCameraComponent::ApplyView(
    const Vector3& cameraPos,
    const Vector3& target)
{
    mCameraPosition = cameraPos;
    mCameraTarget   = target;

    Vector3 eye = cameraPos;
    Vector3 at  = target;
    Vector3 up  = mUpVector;

    Vector3 forward =
        at - eye;

    if (forward.IsZero())
    {
        forward = Vector3::UnitZ;
        at      = eye + forward;
    }

    forward.Normalize();

    const float dotFU =
        Vector3::Dot(
            forward,
            up
        );

    if (std::fabs(dotFU) > 0.99f)
    {
        up = Vector3::UnitX;

        if (std::fabs(
                Vector3::Dot(
                    forward,
                    up
                )) > 0.99f)
        {
            up = Vector3::UnitZ;
        }
    }

    const Matrix4 view =
        Matrix4::CreateLookAt(
            eye,
            at,
            up
        );

    SetViewMatrix(view);
}

} // namespace toy
