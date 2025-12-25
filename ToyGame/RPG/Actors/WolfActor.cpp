#include "WolfActor.h"


WolfActor::WolfActor(toy::Application* a)
: toy::Actor(a)
, mAction(ActionType::IDLE)
, mLifeTime(0.0f)
{
    SetScale(0.05f);
    SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180)));
    
    meshComp = CreateComponent<toy::SkeletalMeshComponent>(1000);
    //meshComp->SetToonRender(true);
    //meshComp->SetContourFactor(1.01f);
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Enemy/wolf.fbx"));
    
    
    mColleder = CreateComponent<toy::ColliderComponent>();
    mColleder->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("Enemy/wolf.fbx")->GetVertexArray());
    mColleder->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 35, 30), Vector3(0.9, 0.9, 0.6));
    mColleder->SetEnabled(true);
    mColleder->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT | toy::C_HURTBOX |  toy::C_ENEMY_TEAM);
    
    
    CreateComponent<toy::GravityComponent>();
    

    auto soundCmomp = CreateComponent<toy::SoundComponent>();
    soundCmomp->SetSound("growling.wav");
    soundCmomp->SetVolume(0.2f);
    soundCmomp->SetLoop(true);
    soundCmomp->Enable3DSound(true);
    soundCmomp->Play();
    
    // 空間に出る文字
    auto textActor = GetApp()->CreateActor<toy::Actor>();
    textActor->SetPosition(Vector3(0.0f, 7.0f, 0.0f));
    auto text = textActor->CreateComponent<toy::TextBillboardComponent>(500);
    text->SetFont(GetApp()->GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 50));
    text->SetColor(Vector3(1.0f, 0.0f, 0.0f));
    text->SetText("Bow \nwow !");

    textActor->SetPosition(GetPosition());
    textActor->SetScale(0.03f);
    
    mTargetActor = GetApp()->CreateActor<toy::Actor>();
    mTarget = mTargetActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::Object2D);
    mTarget->SetTexture(GetApp()->GetAssetManager()->GetTexture("target_scope.png"));
    mTarget->SetBlendAdd(false);
    mTarget->SetIsTopLeft(false);


}

WolfActor::~WolfActor()
{
    
}

void WolfActor::UpdateActor(float deltaTime)
{
    mLifeTime += deltaTime;
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

    mTarget->SetVisible(false);
    if (mColleder->GetTargetState() == toy::TargetState::Candidate)
    {
        auto bb = mColleder->GetBoundingVolume()->GetWorldAABB();
        auto v = (bb.max + bb.min) * 0.5f;

        auto scInfo = GetApp()->GetRenderer()->WorldToScreen(v);
        if (scInfo.visible)
        {
            mTargetActor->SetPosition(Vector3(scInfo.virtualScreen.x, scInfo.virtualScreen.y, 0));
            mTarget->SetVisible(true);
        }
    }
}

void WolfActor::ActionIDLE(float deltaTime)
{
    if (mLifeTime > 5.0f)
    {
        //mAction = ActionType::WALK;
        mLifeTime = 0.0f;
    }
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(2);

}

void WolfActor::ActionWALK(float deltaTime)
{
    if (mLifeTime > 5.0f)
    {
        mAction = ActionType::RUN;
        mLifeTime = 0.0f;
        mLifeTime = 0.0f;
    }
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(1);
}

void WolfActor::ActionRUN(float deltaTime)
{
    if (mLifeTime > 5.0f)
    {
        mAction = ActionType::IDLE;
        mLifeTime = 0.0f;
    }
    auto animPlayer = meshComp->GetAnimPlayer();
    animPlayer->Play(3);
}

