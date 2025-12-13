#include "Movement/DirMoveComponent.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Physics/PhysWorld.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
DirMoveComponent::DirMoveComponent(class Actor* a, int order)
    : MoveComponent(a, order)
    , mSpeed(9.0f)                    // カメラ基準移動の基本速度
    , mPrevPosition(Vector3::Zero)   // 前フレームの位置
{
}

DirMoveComponent::~DirMoveComponent()
{
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・カメラ方向（invViewMatrix）を基準に前後左右移動ベクトルを作成
// ・TryMoveWithRayCheck() で壁すり抜け防止付き移動
// ・位置差分から向き調整（歩いた方向に自動で向く）
//------------------------------------------------------------------------------
void DirMoveComponent::Update(float deltaTime)
{
    // カメラ基準の forward / right ベクトルを取得
    Vector3 forward = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix().GetZAxis();
    Vector3 right   = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix().GetXAxis();

    // Y成分を無視して地面平面に投影
    forward.y = 0.0f;
    right.y   = 0.0f;
    forward.Normalize();
    right.Normalize();

    // 入力速度を合成して移動方向を作成
    Vector3 moveDir = forward * mForwardSpeed + right * mRightSpeed;

    // ノイズ防止：十分に動く時だけ移動
    if (moveDir.LengthSq() > Math::NearZeroEpsilon)
    {
        moveDir.Normalize();
        TryMoveWithRayCheck(moveDir * mSpeed, deltaTime);
    }

    // 実際に動いた方向へ回転させる
    AdjustDir();

    // 次フレーム用に位置を保存
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
