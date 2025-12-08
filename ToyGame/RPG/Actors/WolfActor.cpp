#include "WolfActor.h"

WolfActor::WolfActor(toy::Application* a)
: toy::Actor(a)
{
    SetPosition(Vector3(-20.f, 0.f, 0));
    SetScale(0.1f);
    SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180)));
    
    meshComp = CreateComponent<toy::SkeletalMeshComponent>(1000);
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("wolf.fbx"));
    
    
    auto collComp = CreateComponent<toy::ColliderComponent>();
    collComp->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("wolf.fbx")->GetVertexArray());
    collComp->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 35, 30), Vector3(0.9, 0.9, 0.6));
    collComp->SetDisp(true);
    collComp->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    CreateComponent<toy::GravityComponent>();
    
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(2);

    auto soundCmomp = CreateComponent<toy::SoundComponent>();
    soundCmomp->SetSound("growling.wav");
    soundCmomp->SetLoop(true);
    soundCmomp->SetUseDistanceAttenuation(true);
    soundCmomp->Play();
    
    // 空間に出る文字
    auto textActor = GetApp()->CreateActor<toy::Actor>();
    textActor->SetPosition(Vector3(0.0f, 7.0f, 0.0f));
    auto text = textActor->CreateComponent<toy::TextBillboardComponent>(500);
    text->SetFont(GetApp()->GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 50));
    text->SetColor(Vector3(1.0f, 0.0f, 0.0f));
    text->SetText("Bow wow !");

    textActor->SetParent(this);
    textActor->SetScale(0.03f);
}

WolfActor::~WolfActor()
{
    
}
