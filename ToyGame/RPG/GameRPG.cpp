#include "GameRPG.h"
#include "OutdoorStage.h"
#include "Engine/Core/ApplicationEntry.h"
#include "Actors/HeroActor.h"
#include "Actors/WolfActor.h"

#include "Actors/MinionActor.h"
#include "ToyLib.h"

GameRPG::GameRPG()
: toy::Application()
{
    InitAssetManager("ToyGam/Assets/RPG/", GetRenderer()->GetWindowDisplayScale());
    
    mStage = std::make_unique<OutdoorStage>(this);

}

GameRPG::~GameRPG()
{
    
}

void GameRPG::InitGame()
{
    LoadData();
    
    mStage->InitStage();

    // スプライト
    auto spActor = CreateActor<toy::Actor>();
    //spActor->SetPosition(Vector3(-500.0f, -360.0f, 0.0f));
    spActor->SetPosition(Vector3(0.0f, 700.0f, 0.0f));
    spActor->SetScale(1);
    auto spSprite = spActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::UI);
    spSprite->SetTexture(GetAssetManager()->GetTexture("HealthBar.png"));
    spSprite->SetVisible(true);



}

void GameRPG::LoadData()
{
    
    auto hero = CreateActor<HeroActor>();

    for (int i = 0; i < 5; ++i)
    {
        auto wolf = CreateActor<WolfActor>();
        wolf->SetPosition(Vector3(-20.0f + i*10, 3.0f, -20.0f));
    }

    
    // stan
    auto stanActor = CreateActor<toy::Actor>();
    auto stanMesh = stanActor->CreateComponent<toy::SkeletalMeshComponent>();
    auto stanCllider = stanActor->CreateComponent<toy::ColliderComponent>();
    stanMesh->SetMesh(GetAssetManager()->GetMesh("stan.gltf", true));
    stanMesh->SetToonRender(true);
    
    stanActor->SetPosition(Vector3(-3,0,10));
    stanActor->SetScale(0.5f);
    Quaternion q = Quaternion(Vector3::UnitY, Math::ToRadians(-30));
    stanActor->SetRotation(q);
    
    stanCllider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("stan.gltf")->GetVertexArray());
    stanCllider->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.5, 1.0f, 0.6f));
    stanCllider->GetBoundingVolume()->CreateVArray();
    stanCllider->SetEnabled(true);
    stanCllider->SetFlags(toy::C_WALL | toy::C_ENEMY_TEAM | toy::C_HURTBOX | toy::C_FOOT | toy::C_GROUND);
    
    //auto minionActor = CreateActor<MinionActor>();
    //minionActor->SetParent(hero);
    
   

    auto stanMove = stanActor->CreateComponent<toy::FollowMoveComponent>();
    stanMove->SetTarget(hero);
    stanMove->SetFollowSpeed(10.0f);
    stanActor->CreateComponent<toy::GravityComponent>();


    // BGM
    GetSoundMixer()->LoadBGM("MusMus-BGM-112.mp3");
    GetSoundMixer()->PlayBGM();
    GetSoundMixer()->SetVolume(0.1);
    
    // フォント
    auto fnt = GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 24);
    // テキスト用 Actor を作成
    auto uiActor = CreateActor<toy::Actor>();
    //uiActor->SetPosition(Vector3(600.0f, 360.0f, 0.0f)); // 2Dスクリーン座標として扱う
    uiActor->SetPosition(Vector3(1100.0f, 10.0f, 0.0f)); // 2Dスクリーン座標として扱う

    auto textComp = uiActor->CreateComponent<toy::TextSpriteComponent>();
    textComp->SetFont(fnt);
    textComp->SetFormat("");
    textComp->SetColor(Vector3(1.0f, 1.0f, 0.0f)); // 黄
    mTextComp = textComp;
}


void GameRPG::UpdateGame(float deltaTime)
{
    mStage->Update(deltaTime);
    
    
    auto h = GetTimeOfDaySystem()->GetHour();
    auto m = GetTimeOfDaySystem()->GetMinute();
    mTextComp->SetFormat("時刻 {:02} : {:02}  \n", h, m);
}

void GameRPG::ShutdownGame()
{
    GetSoundMixer()->StopBGM();
    
}
