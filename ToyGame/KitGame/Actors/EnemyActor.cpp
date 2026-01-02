#include "EnemyActor.h"

EnemyActor::EnemyActor(toy::Application* a)
    : toy::Actor(a)
    , mLifeTime(0.0f)
{
    EnemyAction = [this](float deltaTime)
    {
        ActionIDLE(deltaTime);
    };
    
    meshComp = CreateComponent<toy::SkeletalMeshComponent>();
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Monsters/Big/Ninja.gltf"));
    meshComp->SetYawOffset(Math::ToRadians(180.0f));
    

    
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
    
    
    
}

EnemyActor::~EnemyActor()
{
    
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
