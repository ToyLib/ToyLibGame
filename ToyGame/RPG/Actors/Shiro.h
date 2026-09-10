#pragma once

#include "ToyKit.h"

//=============================================================================
// Shiro
//  旧 ShiroActor（toy::kit::KitNpcActor 継承）を、Humanoid Prefab を内包する
//  Game Logic クラスに置き換えた版。
//
//  Humanoid はロックオン戦闘を無効のまま（HumanoidDesc::enableLockOnCombat
//  = false）「体」だけを提供し、索敵→追跡の AI ロジックはこちら側
//  （Game Logic）が持つ（設計方針 8）。Wolf より索敵範囲が広く、やや遅め。
//=============================================================================
class Shiro
{
public:
    explicit Shiro(toy::Application* app);

    // ターゲット（プレイヤー）を設定する。Prefab 越しに位置だけを参照する。
    void SetTarget(toy::kit::Prefab* target) { mTarget = target; }

    void           SetPosition(const Vector3& pos) { mBody.SetPosition(pos); }
    const Vector3& GetPosition() const { return mBody.GetPosition(); }

    void Update(float deltaTime);

private:
    enum class ShiroState { Idle, Chase };

    // アニメーション番号（Shiro.glb のクリップ順）
    static constexpr int ANIM_IDLE = 0;
    static constexpr int ANIM_WALK = 1;
    static constexpr int ANIM_RUN  = 2;

    bool  HasTarget() const { return mTarget != nullptr; }
    float GetDistanceToTarget() const; // XZ 平面距離
    void  MoveTowardTarget(float speed, float dt); // Y は GravityComponent に委譲
    void  LookAtTarget();                          // Y 軸回転のみ

    toy::kit::Humanoid mBody;
    toy::kit::Prefab*   mTarget = nullptr;

    float mDetectRange = 40.0f; // 索敵範囲（XZ距離）
    float mMoveSpeed   = 6.0f;  // 追跡速度
    float mStopRange   = 4.0f;  // 接近停止距離

    toy::kit::KitStateMachine<ShiroState> mFSM;
};
