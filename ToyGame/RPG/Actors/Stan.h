#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Stan
//  Humanoid Prefab + FollowBehavior（プレイヤーに付いて歩くだけのお供NPC）を
//  組み合わせた Agent。演出的な意味は今のところ持たず、ToyKit の Behavior を
//  試すためのテストベットを兼ねる。
//=============================================================================
class Stan : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Stan> MakeStan(toy::Application* app, toy::kit::Prefab* target);
