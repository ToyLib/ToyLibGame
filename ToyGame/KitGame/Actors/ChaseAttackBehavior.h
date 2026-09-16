#pragma once

#include "ToyKit.h"
#include "ChaseAttackBehaviorDesc.h"

//=============================================================================
// ChaseAttackBehavior
//  索敵→追跡→攻撃のAI（IBehavior）。ToyKit::ChaseBehavior と同じ
//  Idle/Chase の考え方に、近づいたら攻撃する Attack ステートを足したもの。
//
//  「Lockon状態」に相当する部分は別ステートにせず、Chase の中に含めている
//  （Chase の onUpdate は毎フレーム FaceTowardPointXZ/MoveTowardPointXZ で
//  ターゲットを追従しており、これ自体がLockon的な振る舞いになっている。
//  一度 Idle→Chase したあとは視野センサーを再チェックしない＝センサーに
//  関係なく追いかけ続ける）。
//
//  Humanoid 固有のメソッド（SetAttackColliderActive 等）を使うため、
//  TobyControlBehavior と同様 Prefab& を Humanoid& にキャストする前提。
//=============================================================================
class ChaseAttackBehavior : public toy::kit::IBehavior
{
public:
    explicit ChaseAttackBehavior(const ChaseAttackBehaviorDesc& desc) : mDesc(desc) {}

    // ターゲットを設定する（Prefab 越しに位置だけを参照する）
    void SetTarget(toy::kit::Prefab* target) { mTarget = target; }

    void OnStart(toy::kit::Prefab& body) override;
    void OnUpdate(toy::kit::Prefab& body, float deltaTime) override;

private:
    enum class State { Idle, Chase, Attack };

    // ターゲットが居て、かつ撃破済みでないこと
    bool HasTarget() const { return mTarget != nullptr && !mTarget->IsDefeated(); }

    ChaseAttackBehaviorDesc mDesc;

    toy::kit::Humanoid* mBody   = nullptr;
    toy::kit::Prefab*   mTarget = nullptr;

    // Chase中、間合い内（attackRange以内）で待機しているかどうか。
    // trueならIdleアニメ、falseならChaseアニメを再生する（状態が変わった時だけ切り替える）。
    bool mWasInAttackRange = false;

    toy::kit::KitStateMachine<State> mFSM;
};
