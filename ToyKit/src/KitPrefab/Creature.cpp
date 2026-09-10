#include "KitPrefab/Creature.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Graphics/Sprite/GroundConformSpriteComponent.h"
#include "Graphics/Billboard/TextBillboardComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
Creature::Creature(toy::Application* app, const CreatureDesc& desc)
    : Prefab(app)
{
    SetupMesh(desc);
    SetupCollider(desc);
    SetupGravity(desc);
    SetupNameBoard(desc);
    SetupTargetSprites(desc);
}

//-----------------------------------------------------------------------------
Creature::~Creature()
{
    if (mNameActor)
    {
        mNameActor->SetState(toy::Actor::State::Dead);
    }
}

//=============================================================================
// Interface
//=============================================================================
void Creature::PlayAnimation(int clipIndex)
{
    if (mMesh) mMesh->GetAnimPlayer()->Play(clipIndex);
}

void Creature::SetVisible(bool visible)
{
    if (mMesh) mMesh->SetVisible(visible);
}

void Creature::SetCollisionEnabled(bool enabled)
{
    if (mCollider) mCollider->SetEnabled(enabled);
}

//=============================================================================
// セットアップ
//=============================================================================
void Creature::SetupMesh(const CreatureDesc& desc)
{
    mMesh = GetActor()->CreateComponent<toy::SkeletalMeshComponent>();

    auto mesh = GetApp()->GetAssetManager()->GetMesh(desc.model);
    mMesh->SetMesh(mesh);

    if (!desc.cancelRootTranslationBone.empty())
    {
        mesh->SetCancelRootTranslation(true);
        mesh->SetCancelNodeName(desc.cancelRootTranslationBone);
    }

    mMesh->SetYawOffset(Math::ToRadians(desc.yawOffsetDeg));
    mMesh->SetToonRender(desc.toonRender);
    mMesh->SetContourFactor(desc.contourFactor);
    mMesh->SetLocalScale(desc.scale);
}

void Creature::SetupCollider(const CreatureDesc& desc)
{
    mCollider = GetActor()->CreateComponent<toy::ColliderComponent>();
    mCollider->GetBoundingVolume()->ComputeFromMeshComponent(mMesh);

    const bool hasOffset = (desc.colliderOffset.x != 0.0f || desc.colliderOffset.y != 0.0f || desc.colliderOffset.z != 0.0f);
    const bool hasScale  = (desc.colliderScale.x  != 1.0f || desc.colliderScale.y  != 1.0f || desc.colliderScale.z  != 1.0f);

    if (hasOffset || hasScale)
    {
        mCollider->GetBoundingVolume()->AdjustBoundingBox(desc.colliderOffset, desc.colliderScale);
    }

    mCollider->SetFlags(desc.colliderFlags);
    mCollider->SetEnabled(true);

    TrackCollider(mCollider);
}

void Creature::SetupGravity(const CreatureDesc& desc)
{
    if (!desc.useGravity) return;

    auto* gravity = GetActor()->CreateComponent<toy::GravityComponent>();
    gravity->SetEnableGroundPose(desc.enableGroundPose);

    TrackGravity(gravity);
}

void Creature::SetupNameBoard(const CreatureDesc& desc)
{
    if (desc.displayName.empty()) return;

    mNameYOffset = desc.nameYOffset;

    // ビルボードをキャラの Actor に持たせるとスケールの影響を受けるため、
    // 独立した Actor に持たせる
    mNameActor = GetApp()->CreateActor<toy::Actor>();

    auto* board = mNameActor->CreateComponent<toy::TextBillboardComponent>(101);
    auto  font  = GetApp()->GetAssetManager()->GetFont(desc.fontPath, 40);
    board->SetFont(font);
    board->SetFormat(desc.displayName);
    board->SetScale(0.01f);
    board->SetColor(desc.nameColor);
}

void Creature::SetupTargetSprites(const CreatureDesc& desc)
{
    if (!desc.candidateTexture.empty())
    {
        mCandidateSigne = CreateTargetSprite(desc.candidateTexture);
    }
    if (!desc.lockedTexture.empty())
    {
        mLockedSigne = CreateTargetSprite(desc.lockedTexture);
    }
}

toy::GroundConformSpriteComponent* Creature::CreateTargetSprite(const std::string& texPath)
{
    auto* sprite = GetActor()->CreateComponent<toy::GroundConformSpriteComponent>();
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

//=============================================================================
// 毎フレーム処理
//=============================================================================
void Creature::OnUpdate(float /*deltaTime*/)
{
    UpdateTargetSprites();
    UpdateNameBoard();
}

void Creature::UpdateTargetSprites()
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

void Creature::UpdateNameBoard()
{
    if (!mNameActor) return;

    const Vector3& pos = GetPosition();
    mNameActor->SetPosition(Vector3(pos.x, pos.y + mNameYOffset, pos.z));
}

} // namespace toy::kit
