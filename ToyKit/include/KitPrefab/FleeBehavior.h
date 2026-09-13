#pragma once

#include "KitPrefab/IBehavior.h"
#include "KitPrefab/FleeBehaviorDesc.h"
#include "KitCore/KitStateMachine.h"

namespace toy::kit {

//=============================================================================
// FleeBehavior
//  索敵→逃走の汎用 AI（IBehavior）。ChaseBehavior の逆で、ターゲットを
//  body の視界センサー（Prefab::HasSensorHit）で捉えたら反対方向へ逃げる。
//  loseDistance より離れたら Idle に戻る（Noriko で最初に使われ、Ninja が
//  2人目の利用者になったためここへ昇格した。ChaseBehavior と同じ流れ）。
//
//  同じ「見つかったら逃げる」相手（Noriko/Ninja 等）は、体の Prefab Desc と
//  この FleeBehaviorDesc の値だけを変えて使い回せる。
//=============================================================================
class FleeBehavior : public IBehavior
{
public:
    explicit FleeBehavior(const FleeBehaviorDesc& desc) : mDesc(desc) {}

    // ターゲットを設定する（Prefab 越しに位置だけを参照する）
    void SetTarget(Prefab* target) { mTarget = target; }

    void OnStart(Prefab& body) override;
    void OnUpdate(Prefab& body, float deltaTime) override;

private:
    enum class State { Idle, Flee };

    bool HasTarget() const { return mTarget != nullptr; }

    FleeBehaviorDesc mDesc;

    Prefab* mBody   = nullptr;
    Prefab* mTarget = nullptr;

    KitStateMachine<State> mFSM;
};

} // namespace toy::kit
