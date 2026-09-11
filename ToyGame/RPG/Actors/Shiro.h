#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Shiro
//  Humanoid Prefab + ChaseBehavior（索敵→追跡の汎用AI）を組み合わせたNPC。
//  行動ロジックそのものは Wolf と共有（ToyKit の ChaseBehavior）で、
//  違うのは Humanoid の見た目と ChaseBehavior のパラメータだけ。
//  Wolf より索敵範囲が広く、やや遅め。
//=============================================================================
class Shiro : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Shiro> MakeShiro(toy::Application* app, toy::kit::Prefab* target);
