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
    

    
    mCollider = CreateComponent<toy::ColliderComponent>();
    mCollider->GetBoundingVolume()->ComputeFromMeshComponent(meshComp);
    mCollider->SetEnabled(true);
    mCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT | toy::C_HURTBOX |  toy::C_ENEMY_TEAM);
    CreateComponent<toy::GravityComponent>();
    
    
    // 足元スプライト
    mLockOnSigne = CreateComponent<toy::GroundConformSpriteComponent>();
    mLockOnSigne->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/lockon.png"));
    mLockOnSigne->SetSize(5, 5);
    mLockOnSigne->SetAlpha(1.0f);
    mLockOnSigne->SetVisible(true);
    mLockOnSigne->SetGroundLift(0.2f);
    mLockOnSigne->SetGridDiv(4);              // まずは4で十分
    mLockOnSigne->SetMaxDeltaFromCenter(0.6f);// ガタつき抑制
    mLockOnSigne->SetBlendAdd(false);

    // 足元スプライト
    mCandidateSigne = CreateComponent<toy::GroundConformSpriteComponent>();
    mCandidateSigne->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/candidate.png"));
    mCandidateSigne->SetSize(5, 5);
    mCandidateSigne->SetBlendAdd(false);
    mCandidateSigne->SetAlpha(0.8f);
    mCandidateSigne->SetVisible(true);
    mCandidateSigne->SetGroundLift(0.2f);
    mCandidateSigne->SetGridDiv(4);              // まずは4で十分
    mCandidateSigne->SetMaxDeltaFromCenter(0.6f);// ガタつき抑制
    
    
    
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
    mNameActor->SetState(toy::Actor::State::Dead);
}

void EnemyActor::UpdateActor(float deltaTime)
{
    mLifeTime += deltaTime;
    EnemyAction(deltaTime);
    
    mCandidateSigne->SetVisible(false);
    mLockOnSigne->SetVisible(false);
    auto pos = GetPosition();
    
    
    if (mCollider->GetTargetState() == toy::TargetState::Candidate)
    {

        mCandidateSigne->SetVisible(true);

    }
    if (mCollider->GetTargetState() == toy::TargetState::Locked)
    {
        mLockOnSigne->SetVisible(true);
    }

    
    
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
