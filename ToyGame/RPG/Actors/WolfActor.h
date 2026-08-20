#pragma once

#include "ToyKit.h"

//=============================================================================
// WolfActor
//  KitCharacterActor を継承したシンプルな敵 NPC。
//  セットアップはヘルパーに任せ、WolfActor 固有の処理だけを記述する。
//=============================================================================

enum class WolfState { Idle, Walk, Run };

class WolfActor : public toy::kit::KitCharacterActor
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
