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
    mCandidateSigne = mTargetActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::Object2D);
    mCandidateSigne->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/candidate_signe.png"));
    mCandidateSigne->SetBlendAdd(false);
    mCandidateSigne->SetIsTopLeft(false);
    mLockOnSigne = mTargetActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::Object2D);
    mLockOnSigne->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/target_scope.png"));
    mLockOnSigne->SetBlendAdd(false);
    mLockOnSigne->SetIsTopLeft(false);
    
    // 足元スプライト
    mTargetSigne = CreateComponent<toy::FootSpriteComponent>();
    mTargetSigne->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/target_signe.png"));
    mTargetSigne->SetSize(5, 5);
    mTargetSigne->SetOffsetPosition(Vector3(0.0f, 0.1f, 0.0f));
    mTargetSigne->SetBlendAdd(true);
    mTargetSigne->SetVisible(false);
    
    
    
    mEnemyName = "Ninja";
    mNameActor = GetApp()->CreateActor<toy::Actor>();
    auto nameBoard = mNameActor->CreateComponent<toy::TextBillboardComponent>(101);
    auto font = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 40);
    nameBoard->SetFont(font);
    nameBoard->SetFormat(mEnemyName);
    nameBoard->SetScale(0.01f);
    nameBoard->SetColor(Vector3(1.0f, 0.0f, 0.0f));
    /*
    auto nameBoardBG = mNameActor->CreateComponent<toy::TextBillboardComponent>(100);
    nameBoardBG->SetFont(font);
    nameBoardBG->SetFormat(mEnemyName);
    nameBoardBG->SetScale(0.0105f);
    nameBoardBG->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    */
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
    
    mCandidateSigne->SetVisible(false);
    mTargetSigne->SetVisible(false);
    mLockOnSigne->SetVisible(false);
    if (mCollider->GetTargetState() == toy::TargetState::Candidate)
    {
        auto v = mCollider->GetCenterPosition();

        auto scInfo = GetApp()->GetRenderer()->WorldToScreen(v);
        if (scInfo.visible)
        {
            mTargetActor->SetPosition(Vector3(scInfo.virtualScreen.x, scInfo.virtualScreen.y, 0));
            //mCandidateSigne->SetVisible(true);
            mTargetSigne->SetVisible(true);
        }
    }
    if (mCollider->GetTargetState() == toy::TargetState::Locked)
    {
        auto v = mCollider->GetCenterPosition();

        auto scInfo = GetApp()->GetRenderer()->WorldToScreen(v);
        if (scInfo.visible)
        {
            mTargetActor->SetPosition(Vector3(scInfo.virtualScreen.x, scInfo.virtualScreen.y, 0));
            //mCandidateSigne->SetVisible(true);
            mTargetSigne->SetVisible(true);
            mLockOnSigne->SetVisible(true);
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
