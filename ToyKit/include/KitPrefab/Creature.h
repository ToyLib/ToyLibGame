#pragma once

#include "KitPrefab/Prefab.h"
#include "KitPrefab/CreatureDesc.h"
#include "ToyLib.h"

namespace toy::kit {

//=============================================================================
// Creature
//  動物・モンスター・異形に使う基本 Prefab（設計方針 4）。
//  Desc からメッシュ・コライダー・重力・名前表示・ターゲット表示を
//  まとめて構築する。ゲーム固有の意味（HP / AI 等）は持たない（設計方針 8）。
//  行動ロジック（索敵・追跡・ステート遷移）は Game Logic 側に置く。
//  名前ビルボード/ターゲット表示スプライトの実装は Prefab 基底が共通で持つ。
//=============================================================================
class Creature : public Prefab
{
public:
    Creature(toy::Application* app, const CreatureDesc& desc);
    ~Creature() override = default;

    //-------------------------------------------------------------------
    // Interface（設計方針 6）
    //-------------------------------------------------------------------
    void PlayAnimation(int clipIndex) override;

    void SetVisible(bool visible) override;
    void SetCollisionEnabled(bool enabled) override;

private:
    void SetupMesh(const CreatureDesc& desc);
    void SetupCollider(const CreatureDesc& desc);
    void SetupGravity(const CreatureDesc& desc);

    toy::SkeletalMeshComponent* mMesh     = nullptr;
    toy::ColliderComponent*     mCollider = nullptr;
};

} // namespace toy::kit
