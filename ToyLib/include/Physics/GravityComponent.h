#pragma once
#include "Engine/Core/Component.h"

namespace toy {

//------------------------------------------------------------------------------
// GravityComponent
//------------------------------------------------------------------------------
// ・Y方向の速度 mVelocityY を持ち、重力加速度を掛け続けるコンポーネント。
// ・PhysWorld::GetNearestGroundY() を使って足元の地面を検出し、
//   貫通しないように Y 位置補正＋接地判定（mIsGrounded）を行う。
// ・Jump() を呼ぶことで、接地中のみ上向き初速（ジャンプ）を与える。
//------------------------------------------------------------------------------
class GravityComponent : public Component
{
public:
    GravityComponent(class Actor* a);
    
    // 毎フレームの重力・接地判定更新
    void Update(float deltaTime) override;
    
    // 接地時に呼ぶとジャンプ初速を与える
    void Jump();
    
    // 重力加速度（負方向の値を推奨）
    void SetGravityAccel(float g) { mGravityAccel = g; }
    
    // ジャンプ初速
    void SetJumpSpeed(float s) { mJumpSpeed = s; }
    float GetJumpSpeed() const { return mJumpSpeed; }
    
    // 現在接地中かどうか
    bool IsGrounded() const { return mIsGrounded; }
    
    // 現在のY方向速度
    float GetVelocityY() const { return mVelocityY; }
    
private:
    // Y方向の速度（正＝上昇、負＝落下）
    float mVelocityY;
    
    // 重力加速度（毎フレーム加算される値）
    float mGravityAccel;
    
    // ジャンプ時の初速
    float mJumpSpeed;
    
    // 落下速度の上限
    float mMaxFallSpeed;
    
    // 接地状態
    bool mIsGrounded;
    
    // 自分の中から C_FOOT フラグを持つ Collider を探すヘルパー
    class ColliderComponent* FindFootCollider();
};

} // namespace toy
