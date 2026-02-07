#include "Movement/DirMoveComponent.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Physics/PhysWorld.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
DirMoveComponent::DirMoveComponent(class Actor* a, int order)
    : MoveComponent(a, order)
{}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・カメラ方向（invViewMatrix）を基準に前後左右移動ベクトルを作成
// ・TryMoveWithRayCheck() で壁すり抜け防止付き移動
// ・位置差分から向き調整（歩いた方向に自動で向く）
//------------------------------------------------------------------------------
void DirMoveComponent::Update(float deltaTime)
{
    mDidMoveByInput = false;

    Vector3 forward = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix().GetZAxis();
    Vector3 right   = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix().GetXAxis();

    forward.y = 0.0f;
    right.y   = 0.0f;
    forward.Normalize();
    right.Normalize();

    // 入力速度（mForwardSpeed/mRightSpeed）は ProcessInput が入れてる前提
    Vector3 moveDir = forward * mForwardSpeed + right * mRightSpeed;

    if (moveDir.LengthSq() > Math::NearZeroEpsilon)
    {
        mDidMoveByInput = true;

        moveDir.Normalize();

        // ★ここ注意：mForwardSpeed に mSpeed を掛けてるなら二重掛けになる
        // その場合は (moveDir * mSpeed) ではなく (moveDir * 1.0f) を渡す
        TryMoveWithRayCheck(moveDir * mSpeed, deltaTime);
    }

    // ★入力で移動したフレームだけ向きを変える
    if (mDidMoveByInput)
    {
        AdjustDir();
    }

    mPrevPosition = GetOwner()->GetPosition();
}
//------------------------------------------------------------------------------
// ProcessInput
//------------------------------------------------------------------------------
// ・左スティック / DPad を前後左右速度に反映
// ・mIsMovable=false のときは入力禁止
//------------------------------------------------------------------------------
void DirMoveComponent::ProcessInput(const struct InputState& state)
{
    if (!mIsMovable) return;

    // 左スティック入力（-1〜+1）
    mForwardSpeed = mSpeed * state.Controller.GetLeftStick().y;
    mRightSpeed   = mSpeed * state.Controller.GetLeftStick().x;


    // DPad補正（キーボード的操作）
    if (state.IsButtonDown(GameButton::DPadLeft))
    {
        mRightSpeed = -mSpeed;
    }
    if (state.IsButtonDown(GameButton::DPadRight))
    {
        mRightSpeed = mSpeed;
    }
    if (state.IsButtonDown(GameButton::DPadUp))
    {
        mForwardSpeed = mSpeed;
    }
    if (state.IsButtonDown(GameButton::DPadDown))
    {
        mForwardSpeed = -mSpeed;
    }
}

//------------------------------------------------------------------------------
// AdjustDir
//------------------------------------------------------------------------------
// ・前フレーム位置との差分から移動方向を算出
// ・その方向へ滑らかに回転（Slerp）
//------------------------------------------------------------------------------
void DirMoveComponent::AdjustDir()
{
    Vector3 moveVal = GetOwner()->GetPosition() - mPrevPosition;
    moveVal.y = 0.0f;  // 水平面のみで回転を判断

    // 一定以上動いていれば回転する
    if (moveVal.LengthSq() > 0.01f)
    {
        float rot = Math::Atan2(moveVal.x, moveVal.z);

        Quaternion targetRot  = Quaternion(Vector3::UnitY, rot);
        Quaternion currentRot = GetOwner()->GetRotation();

        // 徐々に向きを寄せる（0.1 = 回転追従スピード）
        Quaternion smoothRot = Quaternion::Slerp(currentRot, targetRot, 0.1f);

        GetOwner()->SetRotation(smoothRot);
    }
}

} // namespace toy
