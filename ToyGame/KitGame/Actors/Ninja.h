#pragma once

#include "ToyKit.h"
#include <memory>

//=============================================================================
// Ninja
//  Humanoid Prefab + IBehavior を組み合わせた NPC。行動ロジックそのものは
//  専用のものを持たず、Noriko で使っている toy::kit::FleeBehavior と
//  Wolf/Shiro で使っている toy::kit::ChaseBehavior のどちらかを
//  MakeNinja() 呼び出し側が選ぶ（BehaviorType 参照）。
//=============================================================================
class Ninja : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;

    enum class BehaviorType
    {
        Flee,  // Noriko と同じ FleeBehavior（見つかったら逃げる）
        Chase, // Wolf/Shiro と同じ ChaseBehavior（見つかったら追いかける）
    };
};

std::unique_ptr<Ninja> MakeNinja(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                  Ninja::BehaviorType behaviorType);
