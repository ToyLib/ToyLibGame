#include "Movement/OrbitMoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Runtime/InputSystem.h"
#include "Utils/MathUtil.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
OrbitMoveComponent::OrbitMoveComponent(class Actor* owner, int updateOrder)
    : MoveComponent(owner, updateOrder)
    , mCenterActor(nullptr)
    , mOrbitRadius(5.0f)
    , mOrbitLinearSpeed(5.0f)   // 公転時の「歩く速さ」
    , mZoomSpeed(4.0f)          // 近づく/離れる速度
    , mMinRadius(1.0f)
    , mMaxRadius(30.0f)
    , mOrbitInput(0.0f)
    , mZoomInput(0.0f)
    , mCurrentAngle(0.0f)
{
}

//------------------------------------------------------------------------------
// ProcessInput
// ・上下：ターゲットへ近づく/離れる
// ・左右：カメラから見た左右方向に公転
//------------------------------------------------------------------------------
void OrbitMoveComponent::ProcessInput(const struct InputState& state)
{
    if (!mIsMovable) return;

    mOrbitInput = 0.0f;
    mZoomInput  = 0.0f;

    // 左スティック入力（-1〜+1）
    const float stickX = state.Controller.GetLeftStick().x; // 左右
    const float stickY = state.Controller.GetLeftStick().y; // 上下

    constexpr float deadZone = 0.2f;

    if (Math::Abs(stickX) > deadZone)
    {
        mOrbitInput = stickX;      // 右: +1, 左: -1
    }
    if (Math::Abs(stickY) > deadZone)
    {
        // 上（+y）で「近づく」ように符号を反転
        mZoomInput = -stickY;      // 上: +1(近づく), 下: -1(離れる)
    }

    // DPad（キーボード矢印にマップされている想定）
    if (state.IsButtonDown(GameButton::DPadRight))
    {
        mOrbitInput = -1.0f;
    }
    else if (state.IsButtonDown(GameButton::DPadLeft))
    {
        mOrbitInput = 1.0f;
    }

    if (state.IsButtonDown(GameButton::DPadDown))
    {
        mZoomInput = 1.0f;     // 近づく
    }
    else if (state.IsButtonDown(GameButton::DPadUp))
    {
        mZoomInput = -1.0f;    // 離れる
    }
}

//------------------------------------------------------------------------------
// Update
// ・センター周りの円上を「歩く速さ一定」で公転
// ・上下入力で距離を変更
// ・左右入力の向きはカメラから見た左右に合わせる
// ・常にセンターの方向を向く
//------------------------------------------------------------------------------
void OrbitMoveComponent::Update(float deltaTime)
{
    if (!mCenterActor)
    {
        MoveComponent::Update(deltaTime);
        return;
    }

    // カメラ情報（左右方向だけ使う）
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    Vector3 camRight = renderer->GetInvViewMatrix().GetXAxis();
    camRight.y = 0.0f;
    if (camRight.LengthSq() > Math::NearZeroEpsilon)
    {
        camRight.Normalize();
    }

    const Vector3 centerPos = mCenterActor->GetPosition();
    Vector3       selfPos   = GetOwner()->GetPosition();

    // センター → 自分 のベクトル（XZ平面）
    Vector3 radial = selfPos - centerPos;
    radial.y = 0.0f;

    float radius = radial.Length();
    if (radius < 0.001f)
    {
        // ほぼ同じ位置にいるときは適当な向き＆最小半径
        radial = Vector3::UnitZ;
        radius = mOrbitRadius > 0.0f ? mOrbitRadius : mMinRadius;
    }
    else
    {
        if (radius > Math::NearZeroEpsilon)
        {
            radial.Normalize();
        }
    }

    // 現在の半径を更新しておく
    mOrbitRadius = Math::Clamp(radius, mMinRadius, mMaxRadius);

    // 近づく/離れる（半径方向の移動）
    mOrbitRadius += mZoomSpeed * mZoomInput * deltaTime;
    mOrbitRadius = Math::Clamp(mOrbitRadius, mMinRadius, mMaxRadius);

    // 接線方向（XZ平面で radial に直交）
    Vector3 tangent = Vector3::Cross(Vector3::UnitY, radial);
    if (tangent.LengthSq() > Math::NearZeroEpsilon)
    {
        tangent.Normalize();
    }

    // 「右入力で画面右に動く」ように
    // tangent の向きとカメラの right を比較して符号を合わせる
    float dirSign = 1.0f;
    if (Vector3::Dot(tangent, camRight) < 0.0f)
    {
        dirSign = -1.0f;
    }

    // 線速度一定 → 角速度 = v / r
    float angularSpeed = 0.0f;
    if (mOrbitRadius > 0.001f)
    {
        angularSpeed = (mOrbitLinearSpeed / mOrbitRadius); // rad/sec
    }

    // 実際の角度変化（左右入力＆カメラ向きに応じて）
    const float deltaAngle = angularSpeed * (mOrbitInput * dirSign) * deltaTime;
    mCurrentAngle += deltaAngle;

    // radial を Y軸まわりに deltaAngle 回転させる（XZ平面回転）
    const float c = Math::Cos(deltaAngle);
    const float s = Math::Sin(deltaAngle);

    Vector3 newRadial;
    newRadial.x = radial.x * c - radial.z * s;
    newRadial.y = 0.0f;
    newRadial.z = radial.x * s + radial.z * c;

    // 最終位置 = center + newRadial * radius
    Vector3 newPos = centerPos + newRadial * mOrbitRadius;
    // Y は現状維持（地面追従は別コンポーネントに任せる）
    newPos.y = selfPos.y;

    GetOwner()->SetPosition(newPos);

    // 常にセンターの方向を向く（全身後進・横移動っぽい挙動用）
    Vector3 toCenter = centerPos - newPos;
    toCenter.y = 0.0f;

    if (toCenter.LengthSq() > 0.0001f)
    {
        toCenter.Normalize();
        float yaw = Math::Atan2(toCenter.x, toCenter.z);

        Quaternion targetRot  = Quaternion(Vector3::UnitY, yaw);
        Quaternion currentRot = GetOwner()->GetRotation();

        // 少しだけスムーズに寄せる
        Quaternion smoothRot = Quaternion::Slerp(currentRot, targetRot, 0.2f);
        GetOwner()->SetRotation(smoothRot);
    }

    // MoveComponent 側の処理（必要なら）
    MoveComponent::Update(deltaTime);
}

} // namespace toy
