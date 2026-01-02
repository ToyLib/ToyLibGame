#include "EnemyActor.h"

EnemyActor::EnemyActor(toy::Application* a)
    : toy::Actor(a)
    , mLifeTime(0.0f)
    , mEnemyName("Unnamed")
{
    EnemyAction = [this](float deltaTime)
    {
        ActionIDLE(deltaTime);
    };
    
    meshComp = CreateComponent<toy::SkeletalMeshComponent>();
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Monsters/Big/Ninja.gltf"));
    meshComp->SetYawOffset(Math::ToRadians(180.0f));
    meshComp->SetToonRender(true);
    meshComp->SetContourColor(Vector3(0.3f, 0.3f, 0.35f));
    meshComp->SetContourFactor(1.01f);
    

    
    mCollider = CreateComponent<toy::ColliderComponent>();
    mCollider->GetBoundingVolume()->ComputeFromMeshComponent(meshComp);
    mCollider->SetEnabled(true);
    mCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT | toy::C_HURTBOX |  toy::C_ENEMY_TEAM);
    CreateComponent<toy::GravityComponent>();
    
    
    mTargetActor = GetApp()->CreateActor<toy::Actor>();
    mTarget = mTargetActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::Object2D);
    mTarget->SetTexture(GetApp()->GetAssetManager()->GetTexture("target_scope.png"));
    mTarget->SetBlendAdd(false);
    mTarget->SetIsTopLeft(false);
    
    mEnemyName = "Ninja";
    mNameActor = GetApp()->CreateActor<toy::Actor>();
    auto nameBoard = mNameActor->CreateComponent<toy::TextBillboardComponent>();
    auto font = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 40);
    nameBoard->SetFont(font);
    nameBoard->SetFormat(mEnemyName);
    nameBoard->SetScale(0.01f);
    nameBoard->SetColor(Vector3(0.7f, 0.0f, 0.0f));
    
    
    
}

EnemyActor::~EnemyActor()
{
    mTargetActor->SetState(toy::Actor::State::Dead);
    mNameActor->SetState(toy::Actor::State::Dead);
}

void EnemyActor::UpdateActor(float deltaTime)
{
    mLifeTime += deltaTime;
    EnemyAction(deltaTime);
    
    mTarget->SetVisible(false);
    if (mCollider->GetTargetState() == toy::TargetState::Candidate)
    {
        auto bb = mCollider->GetBoundingVolume()->GetWorldAABB();
        auto v = (bb.max + bb.min) * 0.5f;

        auto scInfo = GetApp()->GetRenderer()->WorldToScreen(v);
        if (scInfo.visible)
        {
            mTargetActor->SetPosition(Vector3(scInfo.virtualScreen.x, scInfo.virtualScreen.y, 0));
            mTarget->SetVisible(true);
        }
    }
    Vector3 pos = GetPosition();
    mNameActor->SetPosition(Vector3(pos.x, pos.y+4.0f, pos.z));
}



void EnemyActor::ActionIDLE(float deltaTime)
{
    auto anim = meshComp->GetAnimPlayer();
    anim->Play(3);
    
    if (mLifeTime > 5)
    {
        EnemyAction = [this](float dt)
        {
            ActionWALK(dt);
        };
        mLifeTime = 0.0f;
    }
    
}
void EnemyActor::ActionWALK(float deltaTime)
{
    auto anim = meshComp->GetAnimPlayer();
    anim->Play(10);
    
    if (mLifeTime > 5)
    {
        EnemyAction = [this](float dt)
        {
            ActionRUN(dt);
        };
        mLifeTime = 0.0f;
    }
}
void EnemyActor::ActionRUN(float deltaTime)
{
    auto anim = meshComp->GetAnimPlayer();
    anim->Play(9);
    if (mLifeTime > 5)
    {
        EnemyAction = [this](float dt)
        {
            ActionIDLE(dt);
        };
        mLifeTime = 0.0f;
    }

}
