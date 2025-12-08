#include "BrickActor.h"

BrickActor::BrickActor(toy::Application* a)
: toy::Actor(a)
{
    auto mesh = CreateComponent<toy::MeshComponent>();
    mesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("brick.x"));
    

    SetScale(5.f);
    coll = CreateComponent<toy::ColliderComponent>();
    coll->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("brick.x")->GetVertexArray());
    coll->SetFlags(toy::C_GROUND);
}
