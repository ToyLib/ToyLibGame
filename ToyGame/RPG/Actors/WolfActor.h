#pragma once

#include "ToyKit.h"

//=============================================================================
// WolfActor
//  KitNpcActor を継承した狼型 NPC。
//  Idle 中はターゲットを索敵し、範囲内に入ったら Chase に遷移する。
//=============================================================================

enum class WolfState { Idle, Chase };

class WolfActor : public toy::kit::KitNpcActor
{
public:
    WolfActor(toy::Application* a);
    ~WolfActor() override = default;

protected:
    void UpdateCharacter(float deltaTime) override;

private:
    // アニメーション番号（wolf.gltf のクリップ順）
    static constexpr int ANIM_WALK = 1;
    static constexpr int ANIM_IDLE = 2;
    static constexpr int ANIM_RUN  = 3;

    toy::kit::KitStateMachine<WolfState> mFSM;
};
