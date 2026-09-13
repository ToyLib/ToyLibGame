#pragma once

#include "Skirmisher.h"

//=============================================================================
// Bunny
//  Ninja と同じく Skirmisher に Desc を当てはめただけのキャラ（モデルだけ
//  Monsters/Big/Bunny.gltf に差し替えた版）。Bunny専用のクラスは持たない。
//  Ninja と同様、Flee/Chase のどちらを使うかを呼び出し側が選べる。
//=============================================================================
enum class BunnyBehaviorType
{
    Flee,  // Noriko と同じ FleeBehavior（見つかったら逃げる）
    Chase, // Wolf/Shiro と同じ ChaseBehavior（見つかったら追いかける）
};

std::unique_ptr<Skirmisher> MakeBunny(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                       BunnyBehaviorType behaviorType);
