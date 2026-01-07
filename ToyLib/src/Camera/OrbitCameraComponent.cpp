#include "Camera/OrbitCameraComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"

#include <cmath>
#include <cfloat>

namespace toy {

OrbitCameraComponent::OrbitCameraComponent(Actor* owner)
    : CameraComponent(owner)
    , mOffset(0.0f, 4.0f, -5.0f)   // 初期オフセット（ターゲットのやや上＋後ろ）
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
    //------------------------------------------------------------------
    // 初期距離を mOffset から計算し、ズームの範囲内にクランプ
    //------------------------------------------------------------------
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
    
    //------------------------------------------------------------------
    // Y オフセットも許容範囲にクランプ
    //------------------------------------------------------------------
    if (mOffset.y < mMinOffsetY)
    {
        mOffset.y = mMinOffsetY;
    }
    if (mOffset.y > mMaxOffsetY)
    {
        mOffset.y = mMaxOffsetY;
    }
}

//======================================================================
// OnActivated
//
//  ・他のカメラから Orbit カメラに切り替わった瞬間に呼ばれる
//  ・prevPos    : 直前まで使われていたカメラ位置
//    prevTarget : 直前まで使われていた注視点
//
//  ・「視点のスタート位置」を前カメラと同じ位置に合わせておき、
//    そこから UpdateCamera() 内で Orbit の理想軌道へ補間していく
//======================================================================
void OrbitCameraComponent::OnActivated(const Vector3& prevPos,
                                       const Vector3& prevTarget)
{
    // Orbit が追いかけるターゲット（所有 Actor の頭上あたり）
    Vector3 target = GetOwner()->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);

    // 位置補間の開始地点を前カメラ位置に合わせる
    mCurrentPos    = prevPos;
    mHasCurrentPos = true;

    // 基底クラス側の情報も同期しておく（CameraManager 用）
    mCameraPosition = prevPos;
    mCameraTarget   = target;

    // mOffset / mDistance はいじらない
    // → Orbit 独自の「理想オフセット」はそのまま維持
}

//======================================================================
// ProcessInput
//
//  ・入力状態から「ヨー角速度」と「高さ操作量」を決める
//  ・ここでは値の蓄積のみ行い、実際の適用は UpdateCamera 側で実行
//======================================================================
void OrbitCameraComponent::ProcessInput(const InputState& state)
{
    const float yawSpeedBase = Math::ToRadians(120.0f); // 最大左右回転速度
    
    float yawInput    = 0.0f;
    float heightInput = 0.0f;   // 上を +1 とする
    
    // 将来の右スティック対応（現在はキーボードのみ）
    // const Vector2 rs = state.Controller.GetRightStick();
    // yawInput    += rs.x;
    // heightInput += -rs.y;   // 上を + にしたいので反転
    
    // キーボード入力による回転・高さ操作
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
    
    // 実角速度（rad/s）へ変換
    mYawSpeed    = yawInput * yawSpeedBase;
    mHeightInput = heightInput;
}

//======================================================================
// UpdateCamera
//
//  ・ヨー回転 / 高さ / 距離を更新し理想位置を求める
//  ・前フレーム位置 mCurrentPos から理想位置へ補間
//  ・地面との当たりを考慮したうえで View 行列を適用
//======================================================================
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
    // 2. 高さ更新（Y オフセットのみ変更）
    //--------------------------------
    const float heightSpeed = 7.0f;

    if (std::fabs(mHeightInput) > 1e-4f)
    {
        mOffset.y += mHeightInput * heightSpeed * deltaTime;
        mOffset.y  = Math::Clamp(mOffset.y, mMinOffsetY, mMaxOffsetY);
    }
    // 入力は 1 フレームで消費
    mHeightInput = 0.0f;

    //--------------------------------
    // 3. 高さ → 距離マッピング
    //
    //    ・低いほど近く（nearDist）
    //    ・高いほど遠く（farDist）
    //--------------------------------
    float t = (mOffset.y - mMinOffsetY) / (mMaxOffsetY - mMinOffsetY);
    t = Math::Clamp(t, 0.0f, 1.0f);

    const float nearDist = mMinDistance;
    const float farDist  = mMaxDistance;
    mTargetDistance = nearDist + (farDist - nearDist) * t;

    //--------------------------------
    // 4. 距離をスムーズに追従させる（ズーム補間）
    //--------------------------------
    const float zoomLerpSpeed = 10.0f;
    mDistance += (mTargetDistance - mDistance) * zoomLerpSpeed * deltaTime;
    mDistance  = Math::Clamp(mDistance, mMinDistance, mMaxDistance);

    // オフセットの方向は維持しつつ、距離だけ更新
    Vector3 dir = mOffset;
    if (!dir.IsZero())
    {
        dir.Normalize();
        mOffset = dir * mDistance;
    }

    //--------------------------------
    // 5. 理想位置 & ターゲット算出
    //--------------------------------
    // ターゲット：所有 Actor の頭上あたり
    Vector3 target   = GetOwner()->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);
    Vector3 idealPos = target + mOffset;

    // 初回のみスナップ（外部から OnActivated されていないケース用）
    if (!mHasCurrentPos)
    {
        mCurrentPos    = idealPos;
        mHasCurrentPos = true;
    }

    //--------------------------------
    // 6. 位置補間：前フレーム位置 → 理想位置
    //--------------------------------
    float alpha = mPosLerpSpeed * deltaTime;
    if (alpha > 1.0f) alpha = 1.0f;
    if (alpha < 0.0f) alpha = 0.0f;

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

    // 補正後の位置を次フレーム用に保存
    mCurrentPos = cameraPos;

    //--------------------------------
    // 8. View 行列反映（危険姿勢ガード付き）
    //--------------------------------
    mCameraPosition = cameraPos;
    mCameraTarget   = target;

    Vector3 eye = cameraPos;
    Vector3 at  = target;
    Vector3 up  = mUpVector;

    // forward がゼロベクトルにならないよう防御
    Vector3 forward = at - eye;
    if (forward.IsZero())
    {
        forward = Vector3::UnitZ;
        at      = eye + forward;
    }

    forward.Normalize();
    float dotFU = Vector3::Dot(forward, up);

    // forward と up がほぼ平行な場合は、up を安全な方向に差し替え
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
