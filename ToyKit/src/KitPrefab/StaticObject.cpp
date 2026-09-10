#include "KitPrefab/StaticObject.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Graphics/Mesh/MeshComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
StaticObject::StaticObject(toy::Application* app, const StaticObjectDesc& desc)
    : Prefab(app)
{
    auto mesh = GetApp()->GetAssetManager()->GetMesh(desc.model);

    mMesh = GetActor()->CreateComponent<toy::MeshComponent>();
    mMesh->SetMesh(mesh);
    mMesh->SetToonRender(desc.toonRender);
    mMesh->SetLocalScale(desc.meshScale);

    GetActor()->SetScale(desc.actorScale);

    mCollider = GetActor()->CreateComponent<toy::ColliderComponent>();
    if (desc.colliderFromMeshComponent)
    {
        mCollider->GetBoundingVolume()->ComputeFromMeshComponent(mMesh);
    }
    else
    {
        mCollider->GetBoundingVolume()->ComputeBoundingVolume(mesh->GetVertexArray());
    }

    const bool hasOffset = (desc.colliderOffset.x != 0.0f || desc.colliderOffset.y != 0.0f || desc.colliderOffset.z != 0.0f);
    const bool hasScale  = (desc.colliderScale.x  != 1.0f || desc.colliderScale.y  != 1.0f || desc.colliderScale.z  != 1.0f);
    if (hasOffset || hasScale)
    {
        mCollider->GetBoundingVolume()->AdjustBoundingBox(desc.colliderOffset, desc.colliderScale);
    }

    mCollider->SetFlags(desc.colliderFlags);
    mCollider->SetEnabled(true);
    TrackCollider(mCollider);

    if (desc.useGravity)
    {
        auto* gravity = GetActor()->CreateComponent<toy::GravityComponent>();
        TrackGravity(gravity);
    }
}

//=============================================================================
// Interface
//=============================================================================
void StaticObject::SetVisible(bool visible)
{
    if (mMesh) mMesh->SetVisible(visible);
}

void StaticObject::SetCollisionEnabled(bool enabled)
{
    if (mCollider) mCollider->SetEnabled(enabled);
}

} // namespace toy::kit
