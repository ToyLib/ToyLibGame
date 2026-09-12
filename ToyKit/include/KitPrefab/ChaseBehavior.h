#pragma once

#include "KitPrefab/IBehavior.h"
#include "KitPrefab/ChaseBehaviorDesc.h"
#include "KitCore/KitStateMachine.h"

namespace toy::kit {

//=============================================================================
// ChaseBehavior
//  索敵→追跡の汎用 AI（IBehavior）。
//  ターゲットが detectRange 以内に入ったら追いかけ、
//  detectRange * loseRangeMultiplier より離れたら見失う（ヒステリシス）。
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
