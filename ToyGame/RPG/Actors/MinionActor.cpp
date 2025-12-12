#include "MinionActor.h"

MinionActor::MinionActor(toy::Application* a)
: Actor(a)
, mCounter(0.0f)
{
    auto mesh = CreateComponent<toy::SkeletalMeshComponent>();
    mesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("Enemy_EyeDrone.gltf"));
    SetScale(0.5);
    Quaternion q = Quaternion(Vector3::UnitY, Math::ToRadians(180));
    SetRotation(q);
    
    
    auto animPlayer = mesh->GetAnimPlayer();
    animPlayer->Play(3);
    
    auto light = CreateComponent<toy::PointLightComponent>();
    light->SetColor(Vector3(1.0f, 10.f, 0.0f));
    light->SetIntensity(0.1f);
    light->SetEnabled(false);
    
}

MinionActor::~MinionActor()
{
    
}

void MinionActor::UpdateActor(float deltaTime)
{
    mCounter += deltaTime;
    float x = 2.5f * sin(mCounter*2.5f);
    float y = 2.5f + 0.5f * sin(2.0f * mCounter*2.0f);
    float z = 1.5f + 0.2f * sin(mCounter*3.0f);
    SetPosition(Vector3(x, y, z));
}
