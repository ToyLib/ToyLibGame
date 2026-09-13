#pragma once

#include "Skirmisher.h"

//=============================================================================
// Ninja
//  Skirmisher に「Ninjaの見た目・行動パラメータ」の Desc を当てはめたもの。
//  Ninja 専用のクラスは持たない（Skirmisher.h 参照）。モデルデータだけ違う
//  別キャラを増やすときは、この Ninja.h/.cpp と同じ形で MakeXxxDesc() 一式を
//  用意し、MakeSkirmisher() に渡すだけでよい。
//=============================================================================
enum class NinjaBehaviorType
{
    Flee,  // Noriko と同じ FleeBehavior（見つかったら逃げる）
    Chase, // Wolf/Shiro と同じ ChaseBehavior（見つかったら追いかける）
};

std::unique_ptr<Skirmisher> MakeNinja(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                       NinjaBehaviorType behaviorType);
