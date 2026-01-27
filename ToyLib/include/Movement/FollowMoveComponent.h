#pragma once

#include "Movement/MoveComponent.h"

namespace toy {

//------------------------------------------------------------------------------
// FollowMoveComponent
//------------------------------------------------------------------------------
// ・指定したターゲット Actor を「見る＋付いていく」ための MoveComponent 派生。
// ・ターゲットの方へ向きを回転し、一定距離より離れていれば前進する。
// ・実際の移動は Ray + MTV 付きの TryMoveWithRayCheck() に任せる。
//------------------------------------------------------------------------------
class FollowMoveComponent : public MoveComponent
{
public:
    FollowMoveComponent(class Actor* owner, int updateOrder = 10);
    
    // 追従処理本体（回転＋距離維持＋壁回避付き移動）
    void Update(float deltaTime) override;
    
    // 追従対象の設定
    void SetTarget(class Actor* target)      { mTarget = target; }
    // これ以上離れていたら近づく距離
    void SetFollowDistance(float dist)       { mFollowDistance = dist; }
    // 追従時の移動速度
    void SetFollowSpeed(float speed)         { mFollowSpeed = speed; }
    // 目標方向へ向きを変える最大回転速度（度/秒）
    void SetRotationSpeed(float speed)       { mRotationSpeed = speed; }
    
private:
    // 追従ターゲット
    class Actor* mTarget { nullptr };
    
    // この距離より遠いと前進する
    float mFollowDistance { 3.0f };
    
    // 前進速度
    float mFollowSpeed { 20.0f };
    
    // 1秒あたりの最大回転角（度ベース指定）
    float mRotationSpeed { 45.0f };
};

} // namespace toy
