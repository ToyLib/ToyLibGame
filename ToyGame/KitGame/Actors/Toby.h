#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Toby
//  Humanoid Prefab + TobyControlBehavior（入力処理）を組み合わせた Agent。
//  移動/カメラ切換え/ロックオンの選択・解除ロジックそのものは Humanoid が持つ。
//  TobyControlBehavior はどの入力でどのアニメーションを再生するか
//  （ゲーム固有の意味づけ）だけを担当する。
//=============================================================================
class Toby : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Toby> MakeToby(toy::Application* app);
