#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Wolf
//  Humanoid Prefab + ChaseBehavior（索敵→追跡の汎用AI）を組み合わせたNPC。
//  行動ロジックそのものは Shiro と共有（ToyKit の ChaseBehavior）で、
//  違うのは Humanoid の見た目と ChaseBehavior のパラメータだけ。
//=============================================================================
class Wolf : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Wolf> MakeWolf(toy::Application* app, toy::kit::Prefab* target);
