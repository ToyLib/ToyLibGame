#pragma once

#include "Movement/MoveComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

//------------------------------------------------------------------------------
// InertiaMoveComponent
//------------------------------------------------------------------------------
// ・「慣性あり」の移動を行う MoveComponent 派生。
// ・Forward/Right/Vertical/Angular それぞれに「目標速度」を設定し、
//   Lerp を使って徐々に現在速度に追従させる。
// ・実際の座標更新は MoveComponent::Update() に任せる。
// ・アナログスティックの“ゆっくり開始／ゆっくり終了”などに有効。
//------------------------------------------------------------------------------
class InertiaMoveComponent : public MoveComponent
{
public:
    InertiaMoveComponent(class Actor* owner, int updateOrder = 10);
    
    // 慣性付きの速度更新
    void Update(float deltaTime) override;
    
    //--- 目標速度の設定（外部入力から与える） -------------------------------
    void SetTargetForwardSpeed (float speed) { mTargetForwardSpeed  = speed; }
    void SetTargetRightSpeed   (float speed) { mTargetRightSpeed    = speed; }
    void SetTargetVerticalSpeed(float speed) { mTargetVerticalSpeed = speed; }
    void SetTargetAngularSpeed (float speed) { mTargetAngularSpeed  = speed; }
    
    //--- 加速度パラメータ -----------------------------------------------------
    // 速度がどれくらい早く“目標値”に追従するか。
    void SetAcceleration       (float acc) { mAcceleration        = acc; }
    void SetAngularAcceleration(float acc) { mAngularAcceleration = acc; }
    
private:
    //--- 目標速度（外部から設定される） --------------------------------------
    float mTargetForwardSpeed  {};
    float mTargetRightSpeed    {};
    float mTargetVerticalSpeed {};
    float mTargetAngularSpeed  {};
    
    //--- 慣性の追従速度 --------------------------------------------------------
    float mAcceleration        { 5.0f };  // 直線速度の追従度（高いほどキビキビ）
    float mAngularAcceleration { 90.0f }; // 回転速度の追従度
};

} // namespace toy
