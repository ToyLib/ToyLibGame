#pragma once

#include "KitPrefab/IBehavior.h"
#include "KitPrefab/ChaseBehaviorDesc.h"
#include "KitCore/KitStateMachine.h"

namespace toy::kit {

//=============================================================================
// ChaseBehavior
//  索敵→追跡の汎用 AI（IBehavior）。
//  ターゲットを body の視界センサー（Prefab::HasSensorHit。HumanoidDesc/
//  CreatureDesc の enableVision で有効化する）で捉えたら追いかける。
//  近づきすぎ（stopRange以内）か、loseDistance * loseRangeMultiplier より
//  離れたら Idle に戻る（近づいた後どうするかは今のところ決めておらず、
//  Idle に戻すだけ。実際に攻撃してくる敵が出てから Attack 相当を設計する）。
//
//  同じ「近づいたら追いかけてくる」敵（Wolf/Shiro 等）は、体の Prefab
//  Desc とこの ChaseBehaviorDesc の値だけを変えて使い回せる。
//=============================================================================
class ChaseBehavior : public IBehavior
{
public:
    explicit ChaseBehavior(const ChaseBehaviorDesc& desc) : mDesc(desc) {}

    // ターゲットを設定する（Prefab 越しに位置だけを参照する）
    void SetTarget(Prefab* target) { mTarget = target; }

    void OnStart(Prefab& body) override;
    void OnUpdate(Prefab& body, float deltaTime) override;

private:
    enum class State { Idle, Chase };

    bool HasTarget() const { return mTarget != nullptr; }

    ChaseBehaviorDesc mDesc;

    Prefab* mBody   = nullptr;
    Prefab* mTarget = nullptr;

    KitStateMachine<State> mFSM;
};

} // namespace toy::kit
