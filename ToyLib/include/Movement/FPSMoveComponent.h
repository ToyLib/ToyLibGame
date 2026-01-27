#pragma once
#include "Movement/MoveComponent.h"

namespace toy {

//------------------------------------------------------------------------------
// FPSMoveComponent
//------------------------------------------------------------------------------
// ・FPS視点のキャラクター移動を行う MoveComponent 派生。
// ・左右入力でヨー回転、上下入力で前後移動を行う。
// ・移動時は TryMoveWithRayCheck() を使用し、壁すり抜けを防止する。
//------------------------------------------------------------------------------
class FPSMoveComponent : public MoveComponent
{
public:
    FPSMoveComponent(class Actor* owner, int updateOrder = 10);
   
    // 入力 → 移動量のセット
    void ProcessInput(const struct InputState& state) override;
    
    // 回転＋前後移動（壁判定付き）
    void Update(float deltaTime) override;
    
private:
    float mTurnSpeed { 180.0f };   // 左右回転速度（度/秒）
    float mSpeed     { 7.0f };     // 前進・後退速度
};

} // namespace toy
