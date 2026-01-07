#include "Camera/OrbitCameraComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"
#include <cmath>
#include <cfloat>

#include <iostream>

namespace toy {

OrbitCameraComponent::OrbitCameraComponent(Actor* owner)
    : CameraComponent(owner)
    , mOffset(0.0f, 4.0f, -5.0f)   // 初期オフセット（やや上＋後ろ）
    , mUpVector(Vector3::UnitY)
    , mYawSpeed(0.0f)
    , mDistance(0.0f)
    , mTargetDistance(0.0f)
    , mMinDistance(5.0f)
    , mMaxDistance(20.0f)
    , mMinOffsetY(-2.0f)
    , mMaxOffsetY(8.0f)
    , mHeightInput(0.0f)
    , mCurrentPos(Vector3::Zero)
    , mFirstInterp(true)
    , mHasCurrentPos(false)
    , mPosLerpSpeed(8.0f)
{
    // 初期距離をオフセットから算出し、許容範囲にクランプ
    mDistance = mOffset.Length();
    if (mDistance < mMinDistance)
    {
        mDistance = mMinDistance;
    }
    if (mDistance > mMaxDistance)
    {
        mDistance = mMaxDistance;
    }
    mTargetDistance = mDistance;
    
    // Y オフセットもクランプ
    if (mOffset.y < mMinOffsetY)
    {
        mOffset.y = mMinOffsetY;
    }
    if (mOffset.y > mMaxOffsetY)
    {
        mOffset.y = mMaxOffsetY;
    }
}

//------------------------------------------------------------
// カメラ切り替え時：前カメラの位置からオフセットを再構築
//------------------------------------------------------------
//------------------------------------------------------------
// カメラ切り替え時：前カメラの位置からオフセットを再構築
//------------------------------------------------------------
void OrbitCameraComponent::OnActivated(const Vector3& prevPos,
                                       const Vector3& prevTarget)
{
    // 追いかけるターゲット（主人公の頭上あたり）
    Vector3 target = GetOwner()->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);

    // 補間開始位置を前カメラに合わせる
    mCurrentPos    = prevPos;
    mHasCurrentPos = true;

    // 基底の保持用も同期
    mCameraPosition = prevPos;
    mCameraTarget   = target;

    // ★ mOffset / mDistance はいじらない
    //    → Orbit 独自の「理想オフセット」はそのまま残す
}

void OrbitCameraComponent::ProcessInput(const InputState& state)
{
    // 入力値 → 「1フレーム分のヨー角速度 / 高さ操作」に変換するだけ
    // 実際の適用は Update 側で行う
    
    const float yawSpeedBase = Math::ToRadians(120.0f); // 最大左右回転速度
    
    float yawInput    = 0.0f;
    float heightInput = 0.0f;   // 上を +1 とする
    
    // 将来の右スティック対応（今はコメントアウト）
    // const Vector2 rs = state.Controller.GetRightStick();
    // yawInput    += rs.x;
    // heightInput += -rs.y;   // 上を + にしたいので反転
    
    // キーボード入力
    if (state.IsButtonDown(GameButton::KeyD))
    {
        yawInput += 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyA))
    {
        yawInput -= 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyS))
    {
        heightInput += 1.0f;   // 上方向
    }
    if (state.IsButtonDown(GameButton::KeyW))
    {
        heightInput -= 1.0f;   // 下方向
    }
    
    // 実角速度へ変換（rad/s）
    mYawSpeed    = yawInput * yawSpeedBase;
    mHeightInput = heightInput;
}

void OrbitCameraComponent::UpdateCamera(float deltaTime)
{
    //--------------------------------
    // 1. ヨー回転（水平公転）
    //--------------------------------
    {
        Quaternion yawRot(Vector3::UnitY, mYawSpeed * deltaTime);
        mOffset   = Vector3::Transform(mOffset,   yawRot);
        mUpVector = Vector3::Transform(mUpVector, yawRot);
    }

    //--------------------------------
    // 2. 高さ更新（Yオフセットのみ操作）
    //--------------------------------
    const float heightSpeed = 7.0f;

    if (std::fabs(mHeightInput) > 1e-4f)
    {
        mOffset.y += mHeightInput * heightSpeed * deltaTime;
        mOffset.y  = Math::Clamp(mOffset.y, mMinOffsetY, mMaxOffsetY);
    }
    mHeightInput = 0.0f;

    //--------------------------------
    // 3. 高さ → 距離マッピング
    //--------------------------------
    float t = (mOffset.y - mMinOffsetY) / (mMaxOffsetY - mMinOffsetY);
    t = Math::Clamp(t, 0.0f, 1.0f);

    const float nearDist = mMinDistance;
    const float farDist  = mMaxDistance;
    mTargetDistance = nearDist + (farDist - nearDist) * t;

    //--------------------------------
    // 4. 距離をターゲットに反映
    //--------------------------------
    const float zoomLerpSpeed = 10.0f;
    mDistance += (mTargetDistance - mDistance) * zoomLerpSpeed * deltaTime;
    mDistance  = Math::Clamp(mDistance, mMinDistance, mMaxDistance);

    Vector3 dir = mOffset;
    if (!dir.IsZero())
    {
        dir.Normalize();
        mOffset = dir * mDistance;
    }

    //--------------------------------
    // 5. 理想位置＆ターゲット
    //--------------------------------
    Vector3 target   = GetOwner()->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);
    Vector3 idealPos = target + mOffset;

    // 初回だけスナップ
    if (!mHasCurrentPos)
    {
        mCurrentPos    = idealPos;
        mHasCurrentPos = true;
    }

    //--------------------------------
     // 6. 前フレーム位置 → 理想位置へ補間
     //--------------------------------
     // シンプルな線形補間係数
     float alpha = mPosLerpSpeed * deltaTime;
     if (alpha > 1.0f) alpha = 1.0f;
     if (alpha < 0.0f) alpha = 0.0f; // 一応ガード

     mCurrentPos = Vector3::Lerp(mCurrentPos, idealPos, alpha);

    //--------------------------------
    // 7. 地面との当たり補正（mCurrentPos に対してのみ）
    //--------------------------------
    Vector3 cameraPos = mCurrentPos;

    if (Application* app = GetOwner()->GetApp())
    {
        if (PhysWorld* phys = app->GetPhysWorld())
        {
            float groundY = phys->GetGroundHeightAt(cameraPos);
            if (groundY != -FLT_MAX)
            {
                const float margin = 0.1f;
                float minY = groundY + margin;
                if (cameraPos.y < minY)
                {
                    cameraPos.y = minY;
                }
            }
        }
    }

    // 補正後の位置を次フレームの mCurrentPos に反映
    mCurrentPos = cameraPos;

    //--------------------------------
    // 8. ビュー行列反映（危険姿勢ガード付き）
    //--------------------------------
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
    std::cerr << "cameraPos= " << cameraPos.x << "," << cameraPos.y << "," << cameraPos.z
    << ",target= " << target.x << "," << target.y << "," << target.z
    << ",mOffset= " << mOffset.x << "," << mOffset.y << "," << mOffset.z << ","
    << ",mDistance= " << mDistance << std::endl;
    
}

} // namespace toy
