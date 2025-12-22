#include "IslandActor.h"

IslandActor::IslandActor(toy::Application* a)
: Actor(a)
, mSpeed(3.0f)
, mPosY(0.0f)
, mPosZ(0.0f)
, mLifeTime(0.0f)
{
    auto meshComp = CreateComponent<toy::MeshComponent>();
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("island.x"));
    auto collComp = CreateComponent<toy::ColliderComponent>();
    collComp->SetFlags(toy::C_WALL | toy::C_GROUND);
    collComp->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("island.x")->GetVertexArray());
    SetScale(0.05f);

}

void IslandActor::UpdateActor(float deltaTime)
{
    mLifeTime += deltaTime;
    
    if (mLifeTime < 2.0f)
    {
        mPosY -= mSpeed * deltaTime;
    }
    else if (mLifeTime < 4.0f)
    {
        mPosZ += mSpeed * deltaTime;
    }
    else if (mLifeTime < 6.0f)
    {
        mPosY += mSpeed * deltaTime;
    }
    else if (mLifeTime < 8.0f)
    {
        mPosZ -= mSpeed * deltaTime;
    }
    else
    {
        mLifeTime = 0.0f;
    }


    
    SetPosition(Vector3(30, mPosY, mPosZ));
}
