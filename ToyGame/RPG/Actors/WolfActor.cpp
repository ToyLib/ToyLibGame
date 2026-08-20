#include "WolfActor.h"

WolfActor::WolfActor(toy::Application* a)
    : toy::kit::KitNpcActor(a)
{
    SetScale(3.0f);

    SetupMesh("Enemy/wolf.gltf", 1000);
    SetupCollider(mMesh,
                  toy::C_GROUND | toy::C_WALL | toy::C_FOOT |
                  toy::C_HURTBOX | toy::C_ENEMY_TEAM);
    SetupGravity();
    SetupTargetSprites("target_scope.png");

    //------------------------------------------------------------------
    // Wolf 固有コンポーネント
    //------------------------------------------------------------------
    auto* sound = CreateComponent<toy::SoundComponent>();
    sound->SetSound("growling.wav");
    sound->SetVolume(0.2f);
    sound->SetLoop(true);
    sound->Enable3DSound(true);
    sound->Play();

    auto* text = CreateComponent<toy::TextBillboardComponent>(500);
    text->SetFont(GetApp()->GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 50));
    text->SetColor(Vector3(1.0f, 0.0f, 0.0f));
    text->SetText("Bow \nwow !");
    text->SetScale(0.01f);

    //------------------------------------------------------------------
    // ステートマシン
    //  Idle : 索敵して Chase へ
    //  Chase: 追跡。見失ったら Idle へ（ヒステリシス x1.5）
    //------------------------------------------------------------------
    mFSM.Register(WolfState::Idle,
        /* onUpdate */ [&](float) {
            if (HasTarget() && GetDistanceToTarget() < mDetectRange)
                mFSM.To(WolfState::Chase);
        },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_IDLE); }
    );
    mFSM.Register(WolfState::Chase,
        /* onUpdate */ [&](float dt) {
            MoveTowardTarget(mMoveSpeed, dt);
            LookAtTarget();
            if (!HasTarget() || GetDistanceToTarget() > mDetectRange * 1.5f)
                mFSM.To(WolfState::Idle);
        },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_RUN); }
    );
    mFSM.Start(WolfState::Idle);
}

void WolfActor::UpdateCharacter(float deltaTime)
{
    mFSM.Update(deltaTime);
}
