#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Player
//  Humanoid Prefab + PlayerControlBehavior（入力処理）を組み合わせた Agent。
//  移動/カメラ切換え/ロックオンの選択・解除ロジックそのものは Humanoid が持つ。
//  PlayerControlBehavior はどの入力でどのアニメーションを再生するか
//  （ゲーム固有の意味づけ）だけを担当する。
//=============================================================================
class Player : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Player> MakePlayer(toy::Application* app);
