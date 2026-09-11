#pragma once

#include "ToyKit.h"

//=============================================================================
// Noriko
//  旧 EnemyActor（KitActor 非依存の素の toy::Actor 継承）を、
//  新方針（toy::kit::Prefab を内包する Game Logic クラス）で置き換えた版。
//
//  toy::Actor を継承せず、Creature Prefab をメンバとして持つ。
//  Idle → Walk → Idle のアニメーション切換えだけを行う最小限の振る舞い。
//=============================================================================
class Noriko
{
public:
    Noriko(toy::Application* app, const Vector3& position);

    void Update(float deltaTime);

private:
    enum class MonsterState { Idle, Walk };

    toy::kit::Creature                      mBody;
    toy::kit::KitStateMachine<MonsterState> mFSM;
};
