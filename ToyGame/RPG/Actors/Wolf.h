#pragma once

#include "ToyKit.h"

//=============================================================================
// Wolf
//  旧 WolfActor（toy::kit::KitNpcActor 継承）を、Humanoid Prefab を内包する
//  Game Logic クラスに置き換えた版（Shiro と同型）。
//=============================================================================
class Wolf
{
public:
    explicit Wolf(toy::Application* app);

    // ターゲット（プレイヤー）を設定する。Prefab 越しに位置だけを参照する。
    void SetTarget(toy::kit::Prefab* target) { mTarget = target; }

    void           SetPosition(const Vector3& pos) { mBody.SetPosition(pos); }
    const Vector3& GetPosition() const { return mBody.GetPosition(); }

    void Update(float deltaTime);

private:
    enum class WolfState { Idle, Chase };

    // アニメーション番号（wolf.gltf のクリップ順）
    static constexpr int ANIM_WALK = 1;
    static constexpr int ANIM_IDLE = 2;
    static constexpr int ANIM_RUN  = 3;

    bool  HasTarget() const { return mTarget != nullptr; }
    float GetDistanceToTarget() const;
    void  MoveTowardTarget(float speed, float dt);
    void  LookAtTarget();

    toy::kit::Humanoid mBody;
    toy::kit::Prefab*   mTarget = nullptr;

    float mDetectRange = 30.0f;
    float mMoveSpeed   = 8.0f;
    float mStopRange   = 4.0f;

    toy::kit::KitStateMachine<WolfState> mFSM;
};
