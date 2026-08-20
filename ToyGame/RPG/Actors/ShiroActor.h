#pragma once

#include "ToyKit.h"

//=============================================================================
// ShiroActor
//  KitNpcActor を継承した敵キャラクター（Shiro.glb 専用）。
//  Wolf より索敵範囲が広く、やや遅め。
//=============================================================================

enum class ShiroState { Idle, Chase };

class ShiroActor : public toy::kit::KitNpcActor
{
public:
    ShiroActor(toy::Application* a);
    ~ShiroActor() override = default;

protected:
    void UpdateCharacter(float deltaTime) override;

private:
    // アニメーション番号（Shiro.glb のクリップ順）
    static constexpr int ANIM_IDLE = 0;
    static constexpr int ANIM_WALK = 1;
    static constexpr int ANIM_RUN  = 2;

    toy::kit::KitStateMachine<ShiroState> mFSM;
};
