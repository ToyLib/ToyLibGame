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
//=============================================================================
class Creature : public Prefab
{
public:
    Creature(toy::Application* app, const CreatureDesc& desc);
    ~Creature() override;

    //-------------------------------------------------------------------
    // Interface（設計方針 6）
    //-------------------------------------------------------------------
    void PlayAnimation(int clipIndex);

    void SetVisible(bool visible) override;
    void SetCollisionEnabled(bool enabled) override;

protected:
    void OnUpdate(float deltaTime) override;

private:
    void SetupMesh(const CreatureDesc& desc);
    void SetupCollider(const CreatureDesc& desc);
    void SetupGravity(const CreatureDesc& desc);
    void SetupNameBoard(const CreatureDesc& desc);
    void SetupTargetSprites(const CreatureDesc& desc);

    toy::GroundConformSpriteComponent* CreateTargetSprite(const std::string& texPath);

    void UpdateTargetSprites();
    void UpdateNameBoard();

    toy::SkeletalMeshComponent* mMesh     = nullptr;
    toy::ColliderComponent*     mCollider = nullptr;

    toy::Actor* mNameActor   = nullptr;
    float       mNameYOffset = 4.0f;

    toy::GroundConformSpriteComponent* mCandidateSigne = nullptr;
    toy::GroundConformSpriteComponent* mLockedSigne    = nullptr;
};

} // namespace toy::kit
