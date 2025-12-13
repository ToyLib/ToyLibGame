#include "WolfActor.h"

WolfActor::WolfActor(toy::Application* a)
: toy::Actor(a)
, mAction(ActionType::IDLE)
, mCounter(0)
{
    SetScale(0.05f);
    SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180)));
    
    meshComp = CreateComponent<toy::SkeletalMeshComponent>(1000);
    //meshComp->SetToonRender(true);
    //meshComp->SetContourFactor(1.01f);
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("wolf.fbx"));
    
    
    auto collComp = CreateComponent<toy::ColliderComponent>();
    collComp->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("wolf.fbx")->GetVertexArray());
    collComp->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 35, 30), Vector3(0.9, 0.9, 0.6));
    collComp->SetDisp(true);
    collComp->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    CreateComponent<toy::GravityComponent>();
    

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
    text->SetText("Bow \nwow !");

    textActor->SetParent(this);
    textActor->SetScale(0.03f);
    
    auto light = CreateComponent<toy::PointLightComponent>();
    light->SetColor(Vector3(0.8, 0.8, 1.0f));
}

WolfActor::~WolfActor()
{
    
}

void WolfActor::UpdateActor(float deltaTime)
{
    ++mCounter;
    switch (mAction)
    {
        case ActionType::IDLE:
            ActionIDLE(deltaTime);
            break;
        case ActionType::WALK:
            ActionWALK(deltaTime);
            break;
        case ActionType::RUN:
            ActionRUN(deltaTime);
            break;
        default:
            break;
    }
}

void WolfActor::ActionIDLE(float deltaTime)
{
    if (mCounter % 500 == 0)
    {
        mAction = ActionType::WALK;
    }
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(2);

}

void WolfActor::ActionWALK(float deltaTime)
{
    if (mCounter % 500 == 0)
    {
        mAction = ActionType::RUN;
    }
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(1);
}

void WolfActor::ActionRUN(float deltaTime)
{
    if (mCounter % 500 == 0)
    {
        mAction = ActionType::IDLE;
    }
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(3);
}

