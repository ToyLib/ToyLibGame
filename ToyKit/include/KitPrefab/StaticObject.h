#pragma once

#include "KitPrefab/Prefab.h"
#include "KitPrefab/StaticObjectDesc.h"
#include "ToyLib.h"

namespace toy::kit {

//=============================================================================
// StaticObject
//  建物・岩・家具・障害物に使う基本 Prefab（設計方針 4）。
//  メッシュ+コライダー（+任意で重力）だけを持ち、行動ロジックは一切ない。
//  置く/隠す/当たり判定の有無を切り替える以上のことをしたい場合は
//  Creature 等、行動ロジックを載せられる Prefab を使う。
//=============================================================================
class StaticObject : public Prefab
{
public:
    StaticObject(toy::Application* app, const StaticObjectDesc& desc);
    ~StaticObject() override = default;

    //-------------------------------------------------------------------
    // Interface（設計方針 6）
    //-------------------------------------------------------------------
    void SetVisible(bool visible) override;
    void SetCollisionEnabled(bool enabled) override;

private:
    toy::MeshComponent*     mMesh     = nullptr;
    toy::ColliderComponent* mCollider = nullptr;
};

} // namespace toy::kit
