#include "Movement/InertiaMoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
InertiaMoveComponent::InertiaMoveComponent(class Actor* owner, int updateOrder)
    : MoveComponent(owner, updateOrder)
{
}

//------------------------------------------------------------------------------
// 慣性付きの速度更新
//   - 現在速度 → 目標速度へ徐々に Lerp
//   - 実際の座標反映は MoveComponent::Update() に任せる
//------------------------------------------------------------------------------
void InertiaMoveComponent::Update(float deltaTime)
{
    // 線形速度の追従
    float newForward = Math::Lerp(GetForwardSpeed(),  mTargetForwardSpeed,  mAcceleration * deltaTime);
    float newRight   = Math::Lerp(GetRightSpeed(),    mTargetRightSpeed,    mAcceleration * deltaTime);
    float newVert    = Math::Lerp(GetVerticalSpeed(), mTargetVerticalSpeed, mAcceleration * deltaTime);
    
    // 角速度の追従
    float newAngular = Math::Lerp(GetAngularSpeed(), mTargetAngularSpeed,
                                  mAngularAcceleration * deltaTime);
    
    // MoveComponent の速度として反映
    SetForwardSpeed(newForward);
    SetRightSpeed(newRight);
    SetVerticalSpeed(newVert);
    SetAngularSpeed(newAngular);
    
    // 通常の移動処理（座標更新/回転更新）は MoveComponent に任せる
    MoveComponent::Update(deltaTime);
}

} // namespace toy
