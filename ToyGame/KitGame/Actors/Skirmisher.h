#pragma once

#include "ToyKit.h"
#include <memory>

//=============================================================================
// Skirmisher
//  Humanoid Prefab + IBehavior（Flee/Chase）を組み合わせた汎用NPC。
//  見た目（HumanoidDesc）・行動パラメータ（FleeBehaviorDesc/ChaseBehaviorDesc）は
//  すべて呼び出し側が渡す Desc で決まり、Skirmisher クラス自体はゲーム固有の
//  値を一切持たない。Ninja は、この Skirmisher に「Ninjaの見た目・パラメータ」
//  の Desc を当てはめたものにすぎない（Ninja.h/.cpp 参照。モデルデータだけ
//  違う別キャラを増やすときも、同じように Skirmisher へ別の Desc を当てはめる
//  だけでよく、新しいクラスを作る必要はない）。
//
//  MakeSkirmisher() は FleeBehaviorDesc/ChaseBehaviorDesc のどちらを渡すかで
//  オーバーロード解決され、Flee/Chase のどちらを使うかを選ぶ。
//=============================================================================
class Skirmisher : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;
};

std::unique_ptr<Skirmisher> MakeSkirmisher(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                            const toy::kit::HumanoidDesc& bodyDesc,
                                            const toy::kit::FleeBehaviorDesc& behaviorDesc);

std::unique_ptr<Skirmisher> MakeSkirmisher(toy::Application* app, const Vector3& position, toy::kit::Prefab* target,
                                            const toy::kit::HumanoidDesc& bodyDesc,
                                            const toy::kit::ChaseBehaviorDesc& behaviorDesc);
