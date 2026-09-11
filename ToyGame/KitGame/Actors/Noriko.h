#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Noriko
//  Creature Prefab + IBehavior（Idle/Walkをタイマーで切り替えるだけの
//  最小限の振る舞い）を組み合わせた Agent。
//  Idle → Walk → Idle のアニメーション切換えだけを行う。
//=============================================================================
class Noriko : public toy::kit::Agent<toy::kit::Creature>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position);
