#pragma once
#include "Engine/Core/Component.h"

namespace toy {

//------------------------------------------------------------------------------
// GravityComponent
//------------------------------------------------------------------------------
// ・Y方向の速度 mVelocityY を持ち、重力加速度を積算して落下／上昇を制御するコンポーネント。
// ・PhysWorld::GetNearestGroundY() を使って足元の地面（足場）を検出し、
//   突き抜けないように Actor の Y 位置を補正しつつ、接地状態（mIsGrounded）を管理する。
// ・Jump() を呼ぶと、接地中のみ上向きの初速（ジャンプ速度）を与える。
// ・内部では deltaTime を小さなステップに分割して処理することで、
//   低FPS環境でも接地判定が破綻しにくいようにしている。
//------------------------------------------------------------------------------
class GravityComponent : public Component
{
public:
    GravityComponent(class Actor* a);
    
    // 毎フレームの重力・接地判定更新
    void Update(float deltaTime) override;
    
    // 接地中ならジャンプ初速を与えて空中状態に遷移させる
    void Jump();
    
    // 重力加速度を設定（単位：ユニット/秒^2、下向きなら負の値を推奨）
    void SetGravityAccel(float g) { mGravityAccel = g; }
    
    // ジャンプ初速の設定・取得（単位：ユニット/秒）
    void SetJumpSpeed(float s) { mJumpSpeed = s; }
    float GetJumpSpeed() const { return mJumpSpeed; }
    
    // 現在接地しているかどうか
    bool IsGrounded() const { return mIsGrounded; }
    
    // 現在のY方向速度（単位：ユニット/秒、正＝上昇、負＝落下）
    float GetVelocityY() const { return mVelocityY; }
    
    // パラメータ類（ゲームごとの調整用）
    // ・mMaxFallSpeed    : 落下方向の最大速度（終端速度）
    // ・mMaxStepUp      : 1ステップで「登れる」段差・坂の最大高さ
    // ・mMaxStepDown    : 1ステップで「キャッチ」できる落下距離
    // ・mPenetrationEps : めり込み／浮かせの微調整用の閾値
    void SetMaxFallSpeed(float v)      { mMaxFallSpeed = v; }
    void SetMaxStepUp(float v)         { mMaxStepUp = v; }
    void SetMaxStepDown(float v)       { mMaxStepDown = v; }
    void SetPenetrationEps(float v)    { mPenetrationEps = v; }
    
    void SetEnableGroundPose(bool b)   { mEnableGroundPose = b; }
    bool GetEnableGroundPose() const   { return mEnableGroundPose; }

private:
    // Y方向の速度（正＝上昇、負＝落下）。単位：ユニット/秒
    float mVelocityY;
    
    // 重力加速度。毎ステップ mVelocityY に加算される値（ユニット/秒^2）
    float mGravityAccel;
    
    // ジャンプ時の初速（ユニット/秒）
    float mJumpSpeed;
    
    // 落下速度の下限（負方向の終端速度）
    float mMaxFallSpeed;
    
    // 現在接地状態かどうか
    bool mIsGrounded;
    
    // 地形に合わせてポーズを修正するか
    bool mEnableGroundPose;
    
    // 自分のコンポーネント群から C_FOOT フラグを持つ ColliderComponent を探すヘルパー
    class ColliderComponent* FindFootCollider();
    
    // 分割ステップごとに重力・接地処理を1回進める内部処理
    void StepGravityOnce(float dt, ColliderComponent* collider);
    
    void ApplyGroundPose(Actor* owner, const struct GroundHit& hit, float dt);
    
    // 段差・坂・落下の扱いを決めるパラメータ群
    float mMaxStepUp;        // 登れる最大段差・坂の高さ
    float mMaxStepDown;      // 落下をスナップで拾える最大距離
    float mPenetrationEps;   // めり込み許容／浮かせ量の調整用
};

} // namespace toy
