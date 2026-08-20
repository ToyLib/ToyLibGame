#include "KitActor/KitCharacterActor.h"

#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Graphics/Sprite/GroundConformSpriteComponent.h"
#include "Graphics/Billboard/TextBillboardComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
KitCharacterActor::KitCharacterActor(toy::Application* a)
    : toy::Actor(a)
{
}

//-----------------------------------------------------------------------------
KitCharacterActor::~KitCharacterActor()
{
    // 名前ビルボード用 Actor を自動 Dead 化
    // （EnemyActor のデストラクタで手書きしていたコードが不要になる）
    if (mNameActor)
    {
        mNameActor->SetState(toy::Actor::State::Dead);
    }
}

//-----------------------------------------------------------------------------
// UpdateActor（final）
//  1. 派生クラスの行動ロジック
//  2. ターゲット表示の自動更新
//  3. 名前ビルボードの自動追従
//-----------------------------------------------------------------------------
void KitCharacterActor::UpdateActor(float deltaTime)
{
    UpdateCharacter(deltaTime);
    UpdateTargetSprites();
    UpdateNameBoard();
}

//=============================================================================
// セットアップヘルパー
//=============================================================================

toy::SkeletalMeshComponent* KitCharacterActor::SetupMesh(const std::string& meshPath,
                                                          int order)
{
    mMesh = CreateComponent<toy::SkeletalMeshComponent>(order);
    mMesh->SetMesh(GetApp()->GetAssetManager()->GetMesh(meshPath));
    return mMesh;
}

toy::ColliderComponent* KitCharacterActor::SetupCollider(
    toy::SkeletalMeshComponent* mesh,
    int            flags,
    const Vector3& bboxOffset,
    const Vector3& bboxScale)
{
    mCollider = CreateComponent<toy::ColliderComponent>();
    mCollider->GetBoundingVolume()->ComputeFromMeshComponent(mesh);

    // offset か scale に調整値が入っているときだけ AdjustBoundingBox を呼ぶ
    // （Vector3 に operator== がないため float で比較する）
    const bool hasOffset = (bboxOffset.x != 0.0f || bboxOffset.y != 0.0f || bboxOffset.z != 0.0f);
    const bool hasScale  = (bboxScale.x  != 1.0f || bboxScale.y  != 1.0f || bboxScale.z  != 1.0f);

    if (hasOffset || hasScale)
    {
        mCollider->GetBoundingVolume()->AdjustBoundingBox(bboxOffset, bboxScale);
    }

    mCollider->SetFlags(flags);
    mCollider->SetEnabled(true);
    return mCollider;
}

toy::GravityComponent* KitCharacterActor::SetupGravity()
{
    mGravity = CreateComponent<toy::GravityComponent>();
    return mGravity;
}

void KitCharacterActor::SetupTargetSprites(const std::string& candidateTex,
                                            const std::string& lockedTex)
{
    if (!candidateTex.empty())
    {
        mCandidateSigne = CreateTargetSprite(candidateTex);
    }
    if (!lockedTex.empty())
    {
        mLockedSigne = CreateTargetSprite(lockedTex);
    }
}

void KitCharacterActor::SetupNameBoard(const std::string& name,
                                        const std::string& fontPath,
                                        float              yOffset,
                                        const Vector3&     color)
{
    mNameYOffset = yOffset;

    // 名前表示用に別 Actor を生成（ビルボードをキャラの Actor に持たせると
    // スケールの影響を受けるため、独立した Actor に持たせる）
    mNameActor = GetApp()->CreateActor<toy::Actor>();

    auto* board = mNameActor->CreateComponent<toy::TextBillboardComponent>(101);
    auto  font  = GetApp()->GetAssetManager()->GetFont(fontPath, 40);  // shared_ptr
    board->SetFont(font);
    board->SetFormat(name);
    board->SetScale(0.01f);
    board->SetColor(color);
}

//=============================================================================
// 内部処理
//=============================================================================

toy::GroundConformSpriteComponent*
KitCharacterActor::CreateTargetSprite(const std::string& texPath)
{
    auto* sprite = CreateComponent<toy::GroundConformSpriteComponent>();
    sprite->SetTexture(GetApp()->GetAssetManager()->GetTexture(texPath));
    sprite->SetSize(5, 5);
    sprite->SetBlendAdd(false);
    sprite->SetAlpha(1.0f);
    sprite->SetGroundLift(0.2f);
    sprite->SetGridDiv(4);
    sprite->SetMaxDeltaFromCenter(0.6f);
    sprite->SetVisible(false);
    return sprite;
}

void KitCharacterActor::UpdateTargetSprites()
{
    if (!mCollider) return;

    const auto state = mCollider->GetTargetState();

    if (mCandidateSigne)
    {
        mCandidateSigne->SetVisible(state == toy::TargetState::Candidate);
    }
    if (mLockedSigne)
    {
        mLockedSigne->SetVisible(state == toy::TargetState::Locked);
    }
}

void KitCharacterActor::UpdateNameBoard()
{
    if (!mNameActor) return;

    const Vector3& pos = GetPosition();
    mNameActor->SetPosition(Vector3(pos.x, pos.y + mNameYOffset, pos.z));
}

} // namespace toy::kit
