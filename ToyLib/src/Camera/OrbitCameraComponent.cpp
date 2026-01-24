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

    Vector3 target = ComputeTarget();

    // 「今の mDistance 前提」の理想位置
    Vector3 idealPos = ComputeIdealPos(target);

    //==============================================================
    // 1) 壁衝突：距離（mDistance）をスムーズに詰める/戻す
    //==============================================================
    ApplyWallCollisionDistance(dt, target, idealPos);

    // mDistance が変わったので理想位置を再計算
    idealPos = ComputeIdealPos(target);

    //==============================================================
    // 2) 位置のスムージング
    //==============================================================
    EnsureInitialPos(idealPos);
    ApplyPositionLerp(idealPos, dt);

    Vector3 cameraPos = mCurrentPos;

    //==============================================================
    // 3) 空中Y制御（camera / target を上書き）
    //==============================================================
    mAirY.Apply(GetOwner(), dt, cameraPos, target);

    //==============================================================
    // 4) 遮蔽：target 側へ寄せる（Y逃げ無し）
    //==============================================================
    ResolveWallOcclusion(cameraPos, target, dt);

    //==============================================================
    // 5) 地面：まず target 側へ寄せる。ダメなら最小Y保険。
    //==============================================================
    ResolveGroundCollision(cameraPos, target, dt);

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

    // 高さに応じた “理想ズーム距離”
    mTargetDistance =
        mMinDistance +
        (mMaxDistance - mMinDistance) * t;

    // 通常ズームの追従（壁コリジョンで上書きされる場合もある）
    const float zoomLerpSpeed = 10.0f;

    mDistance += (mTargetDistance - mDistance) * (zoomLerpSpeed * dt);
    mDistance  = Math::Clamp(mDistance, mMinDistance, mMaxDistance);

    // mOffset を距離に合わせて更新（方向は維持）
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
//
//  ・地面に潜りそうなとき「上に逃がす」と被写体を見失いやすい。
//  ・まず “target 方向へ寄せる” ことで解決を試みる。
//  ・それでも潜る場合のみ、最小限のY補正を保険として行う。
//======================================================================
void OrbitCameraComponent::ResolveGroundCollision(Vector3& ioCameraPos,
                                                  const Vector3& target,
                                                  float dt) const
{
    Application* app = GetOwner()->GetApp();
    if (!app) return;

    PhysWorld* phys = app->GetPhysWorld();
    if (!phys) return;

    //==========================================================
    // (1) カメラ直下だけを見る（「直上の床」を拾わない）
    //==========================================================
    const float kStartUp   = 0.05f;   // 少し上から落とす（地面にめり込んでても拾える）
    const float kRayDown   = 50.0f;   // 適当でOK（十分下まで）
    const float kAllowUp   = 0.10f;   // これ以上「上の床」は ground 扱いしない
    const float kMargin    = 0.10f;

    GroundHit hit;
    const Vector3 origin(ioCameraPos.x, ioCameraPos.y + kStartUp, ioCameraPos.z);

    if (!phys->GetGroundHitRayDown(origin, kRayDown, C_GROUND, hit) || !hit.hit)
    {
        return;
    }

    //==========================================================
    // (2) カメラより “上” の地面（上空足場）を無視
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
    // まずは target 方向へ寄せる（見失い軽減）
    //==========================================================
    Vector3 toTarget = target - ioCameraPos;
    const float distToTarget = toTarget.Length();

    if (distToTarget > Math::NearZeroEpsilon)
    {
        toTarget *= (1.0f / distToTarget); // normalize

        const float dy = toTarget.y;

        if (std::fabs(dy) > 1e-6f)
        {
            float s = (minY - ioCameraPos.y) / dy;
            if (s > 0.0f)
            {
                const float maxSlide = distToTarget - 0.05f;
                s = Math::Clamp(s, 0.0f, maxSlide);
                ioCameraPos += toTarget * s;
            }
        }
    }

    //==========================================================
    // 最後の保険：それでも潜ってたら最小限Y補正
    //==========================================================
    if (ioCameraPos.y < minY)
    {
        const float kGroundLerpSpeed = 18.0f;
        float a = kGroundLerpSpeed * dt;
        a = std::min(a, 1.0f);   // 上限
        a = std::max(a, 0.0f);   // 下限

        ioCameraPos.y = ioCameraPos.y + (minY - ioCameraPos.y) * a;
    }
}

//======================================================================
// Wall occlusion
//
//  ・target -> camera の間に壁(C_WALL)があると、被写体が隠れる。
//  ・壁に当たったら「上に逃がす」ではなく「target 側へ寄せる」。
//  ・hit.distance を “距離” として扱い、target→camera の線上で詰める。
//======================================================================
void OrbitCameraComponent::ResolveWallOcclusion(Vector3& ioCameraPos,
                                                const Vector3& target,
                                                float dt)
{
    Application* app = GetOwner()->GetApp();
    if (!app) return;

    PhysWorld* phys = app->GetPhysWorld();
    if (!phys) return;

    Vector3 camDir = ioCameraPos - target; // target -> camera
    float camDist  = camDir.Length();
    if (camDist <= Math::NearZeroEpsilon) return;

    camDir *= (1.0f / camDist); // normalize

    RaycastHit hit{};
    if (!phys->Raycast(target, camDir, camDist, C_WALL, hit))
    {
        return;
    }

    //==========================================================
    // (A) 距離が線分範囲外なら無視（無限レイ対策）
    //==========================================================
    if (!(hit.distance > 0.0f && hit.distance <= camDist))
    {
        return;
    }

    //==========================================================
    // (A2) 近すぎるヒットは無視（開始点付近の誤ヒット/めり込み暴発対策）
    //==========================================================
    const float kMinValidHitDist = std::max(0.05f, mCameraRadius * 0.25f);
    if (hit.distance < kMinValidHitDist)
    {
        return;
    }

    //==========================================================
    // (B) 「壁らしい面」だけ採用（天井/床/直上の下面を弾く）
    //  - 法線が上下寄りなら遮蔽として扱わない
    //==========================================================
    // 壁 = 法線Yが小さい（水平成分が大きい）
    const float kWallNormalYMax = 0.35f; // 0.0に近いほど厳しい / 0.35くらいが無難
    if (std::fabs(hit.normal.y) > kWallNormalYMax)
    {
        return;
    }

    //==========================================================
    // (C) 上空ヒットを無視（遮蔽として意味がある高さだけ採用）
    //==========================================================
    const Vector3 hitPos = target + camDir * hit.distance;

    const float yMin = std::min(target.y, ioCameraPos.y);
    const float yMax = std::max(target.y, ioCameraPos.y);

    const float yPad = mCameraRadius + 0.25f;

    if (hitPos.y < yMin - yPad || hitPos.y > yMax + yPad)
    {
        return;
    }

    //==========================================================
    // 壁の手前に寄せる（上には逃がさない）
    //==========================================================
    const float backOff = mCameraRadius + 0.05f;

    float allowedDist = hit.distance - backOff;
    allowedDist = Math::Clamp(allowedDist, mCollisionMinDistance, camDist);

    const Vector3 desiredPos = target + camDir * allowedDist;

    float alpha = mWallLerpSpeed * dt;
    alpha = Math::Clamp(alpha, 0.0f, 1.0f);

    ioCameraPos = Vector3::Lerp(ioCameraPos, desiredPos, alpha);
}

//======================================================================
// Wall collision limit (pure calculation)
//======================================================================
float OrbitCameraComponent::ResolveWallCollisionLimit(const Vector3& target,
                                                      const Vector3& desiredPos) const
{
    Application* app = GetOwner()->GetApp();
    if (!app) return -1.0f;

    PhysWorld* phys = app->GetPhysWorld();
    if (!phys) return -1.0f;

    Vector3 dir = desiredPos - target;
    float dist  = dir.Length();
    if (dist <= Math::NearZeroEpsilon) return -1.0f;

    dir.Normalize();

    RaycastHit hit{};
    if (!phys->Raycast(target, dir, dist, C_WALL, hit))
    {
        return -1.0f;
    }

    //==========================================================
    // (A) 線分範囲外ヒットは無視
    //==========================================================
    if (!(hit.distance > 0.0f && hit.distance <= dist))
    {
        return -1.0f;
    }

    //==========================================================
    // (A2) 近すぎるヒット無視（開始点付近の暴発対策）
    //==========================================================
    const float kMinValidHitDist = std::max(0.05f, mCameraRadius * 0.25f);
    if (hit.distance < kMinValidHitDist)
    {
        return -1.0f;
    }

    //==========================================================
    // (B) 「壁らしい面」だけ採用（天井/床を弾く）
    //==========================================================
    const float kWallNormalYMax = 0.35f;
    if (std::fabs(hit.normal.y) > kWallNormalYMax)
    {
        return -1.0f;
    }

    //==========================================================
    // (C) 上空ヒット無視
    //==========================================================
    const Vector3 hitPos = target + dir * hit.distance;

    const float yMin = std::min(target.y, desiredPos.y);
    const float yMax = std::max(target.y, desiredPos.y);
    const float yPad = mCameraRadius + 0.25f;

    if (hitPos.y < yMin - yPad || hitPos.y > yMax + yPad)
    {
        return -1.0f;
    }

    float allowed = hit.distance - mCameraRadius;
    allowed = std::max(allowed, mCollisionMinDistance);

    return allowed;
}
//======================================================================
// Apply wall collision distance (smooth)
//
//  ・壁があるとき：allowedDist に向けて “速めに” 縮める
//  ・壁がないとき：mTargetDistance に向けて “遅めに” 戻す
//======================================================================
void OrbitCameraComponent::ApplyWallCollisionDistance(float dt,
                                                      const Vector3& target,
                                                      const Vector3& desiredPos)
{
    float allowedDist = ResolveWallCollisionLimit(target, desiredPos);

    if (allowedDist <= 0.0f)
    {
        // 制限なし：理想距離へゆっくり戻す
        float alpha = mCollisionExpandSpeed * dt;
        alpha = Math::Clamp(alpha, 0.0f, 1.0f);

        mDistance += (mTargetDistance - mDistance) * alpha;
        mDistance  = Math::Clamp(mDistance, mMinDistance, mMaxDistance);
    }
    else
    {
        // 制限あり：壁に当たらない距離へ素早く縮める
        float alpha = mCollisionShrinkSpeed * dt;
        alpha = Math::Clamp(alpha, 0.0f, 1.0f);

        mDistance += (allowedDist - mDistance) * alpha;
        mDistance  = Math::Clamp(mDistance, mMinDistance, mMaxDistance);
    }

    // mOffset を距離に合わせて更新（方向は維持）
    Vector3 dir = mOffset;
    if (dir.IsZero())
    {
        dir = Vector3(0.0f, 0.0f, -1.0f);
    }
    dir.Normalize();
    mOffset = dir * mDistance;
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
