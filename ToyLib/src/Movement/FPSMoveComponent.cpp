#include "Movement/FPSMoveComponent.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Actor.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
FPSMoveComponent::FPSMoveComponent(class Actor* a, int order)
    : MoveComponent(a, order)
{ }

//------------------------------------------------------------------------------
// 入力処理（移動量・回転量を設定）
//------------------------------------------------------------------------------
void FPSMoveComponent::ProcessInput(const struct InputState& state)
{
    if (!mIsMovable)
    {
        return;
    }
    
    // 左スティック：前後・旋回
    mForwardSpeed = mSpeed     * state.Controller.GetLeftStick().y;
    mAngularSpeed = mTurnSpeed * state.Controller.GetLeftStick().x;
    
    // キーボード（DPad）補助入力
    if (state.IsButtonDown(GameButton::DPadLeft))
    {
        mAngularSpeed = -mTurnSpeed;
    }
    if (state.IsButtonDown(GameButton::DPadRight))
    {
        mAngularSpeed =  mTurnSpeed;
    }
    if (state.IsButtonDown(GameButton::DPadUp))
    {
        mForwardSpeed =  mSpeed;
    }
    if (state.IsButtonDown(GameButton::DPadDown))
    {
        mForwardSpeed = -mSpeed;
    }
}

//------------------------------------------------------------------------------
// FPS移動処理（回転→前後移動）
//------------------------------------------------------------------------------
void FPSMoveComponent::Update(float deltaTime)
{
    //------------------------------
    // 1. 回転（左右旋回 / Y軸）
    //------------------------------
    if (!Math::NearZero(mAngularSpeed))
    {
        float angle = Math::ToRadians(mAngularSpeed * deltaTime);
        Quaternion inc(Vector3::UnitY, angle);
        Quaternion rot = Quaternion::Concatenate(GetOwner()->GetRotation(), inc);
        GetOwner()->SetRotation(rot);
    }
    
    //------------------------------
    // 2. 前進・後退（RayCCDつき）
    //------------------------------
    if (!Math::NearZero(mForwardSpeed))
    {
        Vector3 moveDir = GetOwner()->GetForward();
        moveDir.y = 0.0f; // 水平移動のみ
        
        if (moveDir.LengthSq() > Math::NearZeroEpsilon)
        {
            moveDir.Normalize();
            Vector3 displacement = moveDir * mForwardSpeed;
            
            // 壁判定＋押し戻しを含む安全移動
            TryMoveWithRayCheck(displacement, deltaTime);
        }
    }
}

} // namespace toy
