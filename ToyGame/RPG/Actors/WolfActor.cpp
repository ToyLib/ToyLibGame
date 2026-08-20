#include "WolfActor.h"

WolfActor::WolfActor(toy::Application* a)
    : toy::kit::KitCharacterActor(a)
{
    SetScale(3.0f);

    //------------------------------------------------------------------
    // KitCharacterActor のヘルパーでキャラクター基盤を一括セットアップ
    //------------------------------------------------------------------
    SetupMesh("Enemy/wolf.gltf", 1000);

    SetupCollider(mMesh,
                  toy::C_GROUND | toy::C_WALL | toy::C_FOOT |
                  toy::C_HURTBOX | toy::C_ENEMY_TEAM);

    SetupGravity();

    // candidate 表示のみ（locked 表示は持たない）
    SetupTargetSprites("target_scope.png");

    //------------------------------------------------------------------
    // Wolf 固有のコンポーネント
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
    //------------------------------------------------------------------
    mFSM.Register(WolfState::Idle,
        /* onUpdate */ [&](float) { if (mFSM.GetTimer() > 5.0f) mFSM.To(WolfState::Walk); },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_IDLE); }
    );
    mFSM.Register(WolfState::Walk,
        /* onUpdate */ [&](float) { if (mFSM.GetTimer() > 5.0f) mFSM.To(WolfState::Run); },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_WALK); }
    );
    mFSM.Register(WolfState::Run,
        /* onUpdate */ [&](float) { if (mFSM.GetTimer() > 5.0f) mFSM.To(WolfState::Idle); },
        /* onEnter  */ [&]{ mMesh->GetAnimPlayer()->Play(ANIM_RUN); }
    );
    mFSM.Start(WolfState::Idle);
}

void WolfActor::UpdateCharacter(float deltaTime)
{
    mFSM.Update(deltaTime);
    // ターゲット表示は KitCharacterActor::UpdateActor が自動で処理する
}
