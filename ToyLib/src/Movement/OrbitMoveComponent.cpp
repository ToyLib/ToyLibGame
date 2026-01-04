#include "Movement/OrbitMoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Runtime/InputSystem.h"
#include "Utils/MathUtil.h"

namespace toy {

OrbitMoveComponent::OrbitMoveComponent(class Actor* owner, int updateOrder)
: MoveComponent(owner, updateOrder)
, mCenterActor(nullptr)
, mOrbitRadius(5.0f)
, mMinRadius(1.0f)
, mMaxRadius(30.0f)
, mMaxOrbitSpeed(5.0f)    // 公転の最大速度 [unit/sec]
, mMaxRadialSpeed(6.0f)   // 近づく/離れるの最大速度
, mCurrentAngle(0.0f)
{
}

//----------------------------------------
// ProcessInput
//  ・上下：中心へ近づく/離れる → mForwardSpeed に半径方向速度をセット
//  ・左右：カメラ左右基準の公転 → mRightSpeed に円周方向速度をセット
//----------------------------------------
void OrbitMoveComponent::ProcessInput(const struct InputState& state)
{
    if (!mIsMovable) return;
    
    // ここでは「速度を決めるだけ」。Update では一切書き換えない。
    mForwardSpeed = 0.0f;
    mRightSpeed   = 0.0f;
    mAngularSpeed = 0.0f; // このコンポーネントでは使わない（常に中心を向く）
    
    // 左スティック
    const float sx = -state.Controller.GetLeftStick().x; // 左右
    const float sy = -state.Controller.GetLeftStick().y; // 上下
    constexpr float deadZone = 0.2f;
    
    if (Math::Abs(sx) > deadZone)
    {
        // 右入力で +mMaxOrbitSpeed、左で -mMaxOrbitSpeed
        mRightSpeed = mMaxOrbitSpeed * sx;
    }
    if (Math::Abs(sy) > deadZone)
    {
        // 上(+y)で中心に近づく（+）、下で離れる（-）
        mForwardSpeed = mMaxRadialSpeed * sy;
    }
    
    // DPad（キーボード矢印想定）
    if (state.IsButtonDown(GameButton::DPadLeft))
    {
        mRightSpeed = mMaxOrbitSpeed;
    }
    else if (state.IsButtonDown(GameButton::DPadRight))
    {
        mRightSpeed =  -mMaxOrbitSpeed;
    }
    
    if (state.IsButtonDown(GameButton::DPadUp))
    {
        mForwardSpeed =  -mMaxRadialSpeed;   // 近づく
    }
    else if (state.IsButtonDown(GameButton::DPadDown))
    {
        mForwardSpeed = mMaxRadialSpeed;   // 離れる
    }
}

//----------------------------------------
// Update
//  ・mForwardSpeed : 半径方向の速度 → 距離 r を増減
//  ・mRightSpeed   : 円周方向の速度 → 公転の角度を変化
//  ・左右向きは「カメラから見た左右」に合わせる
//  ・常にセンターを向く（ロックオン）
//----------------------------------------
void OrbitMoveComponent::Update(float deltaTime)
{
    
    if (!mIsMovable) return;
    if (!mCenterActor)
    {
        // 中心がいないときは何もしない or MoveComponent::Update を呼ぶかは好み
        return;
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // 画面右方向（XZ平面）
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
    if (radius < Math::NearZeroEpsilon)
    {
        // ほぼ同位置にいるときは適当な初期向き＆最小半径
        radial = Vector3::UnitZ;
        radius = (mOrbitRadius > 0.0f) ? mOrbitRadius : mMinRadius;
    }
    else
    {
        radial.Normalize();   // ★ /= ではなく Normalize()
    }

    // いまの半径を基準に
    mOrbitRadius = Math::Clamp(radius, mMinRadius, mMaxRadius);

    //==============================
    // 1) 半径方向の移動（近づく / 離れる）
    //    mForwardSpeed をそのまま「半径の速度」として使う
    //==============================
    mOrbitRadius += mForwardSpeed * deltaTime;
    mOrbitRadius = Math::Clamp(mOrbitRadius, mMinRadius, mMaxRadius);

    //==============================
    // 2) 円周方向の移動（公転）
    //    mRightSpeed を「円周方向の線速度」として使う
    //==============================
    // 接線方向（XZ平面で radial に直交）
    Vector3 tangent = Vector3::Cross(Vector3::UnitY, radial);
    if (tangent.LengthSq() > Math::NearZeroEpsilon)
    {
        tangent.Normalize();
    }

    // 「右入力（mRightSpeed > 0）で画面右に動く」ように向きを揃える
    float camAlignSign = 1.0f;
    if (Vector3::Dot(tangent, camRight) < 0.0f)
    {
        camAlignSign = -1.0f;
    }

    const float orbitSpeed = mRightSpeed;        // [unit/sec]（符号付き）
    const float speedAbs   = Math::Abs(orbitSpeed);

    float deltaAngle = 0.0f;
    if (mOrbitRadius > 0.001f && speedAbs > Math::NearZeroEpsilon)
    {
        // 線速度一定 → 角速度 = v / r
        float angularSpeed = speedAbs / mOrbitRadius; // [rad/sec]

        // 正の速度 → 画面右方向に回るように符号を決める
        float sign = (orbitSpeed >= 0.0f) ? 1.0f : -1.0f;
        deltaAngle = angularSpeed * deltaTime * sign * camAlignSign;
    }

    // Y軸回りに radial を回転
    const float c = Math::Cos(deltaAngle);
    const float s = Math::Sin(deltaAngle);

    Vector3 newRadial;
    newRadial.x = radial.x * c - radial.z * s;
    newRadial.y = 0.0f;
    newRadial.z = radial.x * s + radial.z * c;

    // 最終位置 = center + newRadial * radius
    Vector3 newPos = centerPos + newRadial * mOrbitRadius;

    // Yはそのまま（地面追従は別コンポーネント）
    newPos.y = selfPos.y;
    GetOwner()->SetPosition(newPos);

    //==============================
    // 3) 常にセンターの方向を向く（ロックオン）
    //==============================
    Vector3 toCenter = centerPos - newPos;
    toCenter.y = 0.0f;

    if (toCenter.LengthSq() > 0.0001f)
    {
        toCenter.Normalize();
        float yaw = Math::Atan2(toCenter.x, toCenter.z);

        Quaternion targetRot  = Quaternion(Vector3::UnitY, yaw);
        Quaternion currentRot = GetOwner()->GetRotation();

        Quaternion smoothRot = Quaternion::Slerp(currentRot, targetRot, 0.2f);
        GetOwner()->SetRotation(smoothRot);
    }

    // ※ MoveComponent::Update(deltaTime) は呼ばない。
    //   （呼ぶと mForwardSpeed/mRightSpeed による「通常移動」と二重で動いてしまう）
}

} // namespace toy
