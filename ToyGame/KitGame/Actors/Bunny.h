#pragma once

#include "Skirmisher.h"

//=============================================================================
// Bunny
//  Ninja と同じく Skirmisher に Desc を当てはめただけのキャラ（モデルだけ
//  Monsters/Big/Bunny.gltf に差し替えた版）。Bunny専用のクラスは持たない。
//  ウサギらしく FleeBehavior 固定（Ninjaのように Chase を選ぶ余地は無い）。
//=============================================================================
std::unique_ptr<Skirmisher> MakeBunny(toy::Application* app, const Vector3& position, toy::kit::Prefab* target);
