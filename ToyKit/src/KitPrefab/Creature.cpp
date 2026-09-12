#include "KitPrefab/Creature.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
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

    if (!desc.displayName.empty())
    {
        SetupNameBoard(desc.displayName, desc.fontPath, desc.nameYOffset, desc.nameColor);
    }
    SetupTargetSprites(desc.candidateTexture, desc.lockedTexture);

    if (desc.enableSensor)
    {
        SetupSensor(desc.sensorFovDeg, desc.sensorMaxDist, desc.sensorTargetMask, desc.sensorRequireLOS);
    }
}

//=============================================================================
// Interface
//=============================================================================
void Creature::PlayAnimation(int clipIndex)
{
    if (mMesh) mMesh->GetAnimPlayer()->Play(clipIndex);
}

void Creature::PlayAnimationBlend(int clipIndex, float blendDuration)
{
    if (!mMesh) return;
    auto* anim = mMesh->GetAnimPlayer();
    anim->PlayBlend(anim->GetAnimID(), clipIndex, blendDuration);
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

} // namespace toy::kit
