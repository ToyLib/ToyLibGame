#pragma once

#include "KitPrefab/StaticObjectDesc.h"
#include "Utils/MathUtil.h"

#include <memory>
#include <vector>

namespace toy { class Application; }

namespace toy::kit {

class StaticObject;

//=============================================================================
// StaticObjectPlacement
//  StaticObject を1体、どこに・どう向けて置くかをまとめたデータ（設計方針5/16）。
//  Desc と同じく実行時参照を持たないプリミティブ構造体——Scene構築のDesc化の
//  最小単位。Scene 側は「procedural に1体ずつ生成する」代わりに、この構造体の
//  配列（データ）を組み立てて MakeStaticObjects() に渡すだけでよい。
//=============================================================================
struct StaticObjectPlacement
{
    StaticObjectDesc desc;
    Vector3          position = Vector3::Zero;
    Quaternion       rotation = Quaternion::Identity;
};

// Placement 1件から StaticObject を1体組み立てる
std::unique_ptr<StaticObject> MakeStaticObject(toy::Application* app, const StaticObjectPlacement& placement);

// Placement のリストから StaticObject をまとめて組み立てる
std::vector<std::unique_ptr<StaticObject>> MakeStaticObjects(toy::Application* app,
                                                              const std::vector<StaticObjectPlacement>& placements);

} // namespace toy::kit
