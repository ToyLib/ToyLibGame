#include "WolfActor.h"


WolfActor::WolfActor(toy::Application* a)
: toy::Actor(a)
, mAction(ActionType::IDLE)
, mLifeTime(0.0f)
{
    SetScale(3.0f);
    //SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180)));
    
    meshComp = CreateComponent<toy::SkeletalMeshComponent>(1000);
    //meshComp->SetToonRender(true);
    //meshComp->SetContourFactor(1.01f);
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Enemy/wolf.gltf"));
    
    
    mColleder = CreateComponent<toy::ColliderComponent>();
    mColleder->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("Enemy/wolf.gltf")->GetVertexArray());
    //mColleder->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 35, 30), Vector3(0.9, 0.9, 0.6));
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
    auto text = CreateComponent<toy::TextBillboardComponent>(500);
    text->SetFont(GetApp()->GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 50));
    text->SetColor(Vector3(1.0f, 0.0f, 0.0f));
    text->SetText("Bow \nwow !");
    text->SetScale(0.01f);

    
    mTargetActor = GetApp()->CreateActor<toy::Actor>();
    mTarget = CreateComponent<toy::GroundConformSpriteComponent>();
    mTarget->SetTexture(GetApp()->GetAssetManager()->GetTexture("target_scope.png"));
    mTarget->SetBlendAdd(false);
    mTarget->SetSize(5, 5);
    mTarget->SetAlpha(1.0f);
    mTarget->SetGroundLift(0.2f);
    mTarget->SetGridDiv(4);              // まずは4で十分
    mTarget->SetMaxDeltaFromCenter(0.6f);// ガタつき抑制

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
        mTarget->SetVisible(true);
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

