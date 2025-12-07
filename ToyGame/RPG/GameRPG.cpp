#include "GameRPG.h"
#include "Engine/Core/ApplicationEntry.h"
#include "HeroActor.h"
#include "MinionActor.h"
#include "ToyLib.h"

// ToyLibの起動Applicationとして登録
TOYLIB_REGISTER_APP(GameRPG)


GameRPG::GameRPG()
: toy::Application()
{
    InitAssetManager("ToyGame/Assets/RPG/", GetRenderer()->GetWindowDisplayScale());
    
    GetTimeOfDaySystem()->SetTimeScale(1.f);
    GetTimeOfDaySystem()->SetTime(16);
}

GameRPG::~GameRPG()
{
    
}

void GameRPG::InitGame()
{
    LoadData();
    


    // スプライト
    auto spActor = CreateActor<toy::Actor>();
    //spActor->SetPosition(Vector3(-500.0f, -360.0f, 0.0f));
    spActor->SetPosition(Vector3(0.0f, 700.0f, 0.0f));
    spActor->SetScale(1);
    auto spSprite = spActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::UI);
    spSprite->SetTexture(GetAssetManager()->GetTexture("HealthBar.png"));
    spSprite->SetVisible(true);

    
    // 木（ビルボード）
    auto treeActor = CreateActor<toy::Actor>();
    treeActor->SetPosition(Vector3(20.0f, 4.5f, 0.0f));
    treeActor->SetScale(0.02);
    auto treeBillboard = treeActor->CreateComponent<toy::BillboardComponent>(100);
    treeBillboard->SetTexture(GetAssetManager()->GetTexture("tree.png"));
    treeBillboard->SetVisible(true);
    treeActor->CreateComponent<toy::GravityComponent>();
    auto treeCollider = treeActor->CreateComponent<toy::ColliderComponent>();
    treeCollider->GetBoundingVolume()->ComputeBoundingVolume(Vector3(20, -256, -4), Vector3(40,200,4));
    treeCollider->SetFlags(toy::C_WALL | toy::C_FOOT);
    
    // シャドウ用スプライト
    auto shadow = treeActor->CreateComponent<toy::ShadowSpriteComponent>(10);
    shadow->SetVisible(true);
    shadow->SetOffsetPosition(Vector3(0.0f, -4.5f, 0.0f));
    shadow->SetOffsetScale(0.03f);
    
    
    
    // 焚き火
    auto fireActor = CreateActor<toy::Actor>();
    auto fireMesh = fireActor->CreateComponent<toy::MeshComponent>();
    fireMesh->SetMesh(GetAssetManager()->GetMesh("campfile.x"));
  
    fireActor->SetPosition(Vector3(-8, 0, -30));
    fireActor->SetScale(0.03f);
    auto fireCollider = fireActor->CreateComponent<toy::ColliderComponent>();
    fireCollider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("campfile.x")->GetVertexArray());
    fireCollider->SetDisp(true);
    fireCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    fireActor->CreateComponent<toy::GravityComponent>();
    
    auto fireSound = fireActor->CreateComponent<toy::SoundComponent>();
    fireSound->SetSound("fire.wav");
    fireSound->SetLoop(true);
    fireSound->SetVolume(0.5f);
    fireSound->SetUseDistanceAttenuation(true);
    fireSound->Play();


    
    
    // 炎
    auto particleActor = CreateActor<toy::Actor>();
    particleActor->SetPosition(Vector3(0, 0, 0));
    auto particleComp = particleActor->CreateComponent<toy::ParticleComponent>();
    particleComp->SetTexture(GetAssetManager()->GetTexture("fire.png"));
    particleComp->CreateParticles(Vector3(0, 0, 0),
                                  10,
                                  1000,
                                  0.3f,
                                  5.5,
                                  toy::ParticleComponent::P_SMOKE);
    particleComp->SetAddBlend(true);
    particleActor->SetParent(fireActor);

    

    
    /*
    auto sunlightActor = CreateActor<Actor>();
    auto sunlight = sunlightActor->CreateComponent<SpriteComponent>(100, VisualLayer::UI);
    std::shared_ptr<Texture> lightTex(new Texture);
    lightTex->CreateRadialRays(1024, 16, 3.f, 2.0f, 0.15f);
    sunlight->SetTexture(lightTex);
    */
}

void GameRPG::LoadData()
{
    
    auto hero = CreateActor<HeroActor>();
    
    // stan
    auto stanActor = CreateActor<toy::Actor>();
    auto stanMesh = stanActor->CreateComponent<toy::SkeletalMeshComponent>();
    auto stanCllider = stanActor->CreateComponent<toy::ColliderComponent>();
    stanMesh->SetMesh(GetAssetManager()->GetMesh("stan.gltf", true));
//    stanMesh->SetToonRender(true, 1.015f);
    
    stanActor->SetPosition(Vector3(-3,0,10));
    stanActor->SetScale(0.5f);
    Quaternion q = Quaternion(Vector3::UnitY, Math::ToRadians(-30));
    stanActor->SetRotation(q);
    
    stanCllider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("stan.gltf")->GetVertexArray());
    stanCllider->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 0, 0), Vector3(0.5, 1.f, 0.6));
    stanCllider->GetBoundingVolume()->CreateVArray();
    stanCllider->SetDisp(true);
    stanCllider->SetFlags(toy::C_WALL | toy::C_ENEMY | toy::C_FOOT | toy::C_GROUND);
    
    auto minionActor = CreateActor<MinionActor>();
    minionActor->SetParent(hero);
    
    
    // wolf
    auto wolfActor = CreateActor<toy::Actor>();
    auto wolfMesh = wolfActor->CreateComponent<toy::SkeletalMeshComponent>(2000);
    wolfMesh->SetMesh(GetAssetManager()->GetMesh("wolf.fbx"));

    wolfActor->SetPosition(Vector3(-20.f, 0.f, 0));
    wolfActor->SetScale(0.1f);
    q = Quaternion(Vector3::UnitY, Math::ToRadians(180));
    wolfActor->SetRotation(q);
    auto wolfCollider = wolfActor->CreateComponent<toy::ColliderComponent>();
    wolfCollider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("wolf.fbx")->GetVertexArray());
    wolfCollider->GetBoundingVolume()->AdjustBoundingBox(Vector3(0.0f, 35, 30), Vector3(0.9, 0.9, 0.6));
    wolfCollider->SetDisp(true);
    wolfCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    wolfActor->CreateComponent<toy::GravityComponent>();
    auto animPlayer = wolfMesh->GetAnimPlayer();
    animPlayer->Play(2);

    auto wolfSound = wolfActor->CreateComponent<toy::SoundComponent>();
    wolfSound->SetSound("growling.wav");
    wolfSound->SetLoop(true);
    wolfSound->SetUseDistanceAttenuation(true);
    wolfSound->Play();
        
    // 空間に出る文字
    auto stanTextActor = CreateActor<toy::Actor>();
    stanTextActor->SetPosition(Vector3(0.0f, 7.0f, 0.0f));
    auto stanText = stanTextActor->CreateComponent<toy::TextBillboardComponent>(500);
    auto stanFont = GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 50);
    stanText->SetFont(stanFont);
    stanText->SetColor(Vector3(1.0f, 0.0f, 0.0f));
    stanText->SetText("Bow wow !");
    stanTextActor->SetParent(wolfActor);
    stanTextActor->SetScale(0.03f);
    
    // cat
    auto catActor = CreateActor<toy::Actor>();
    catActor->SetPosition(Vector3(20.f, -5.f, 10.f));
    catActor->SetScale(0.1f);
    auto catMesh = catActor->CreateComponent<toy::SkeletalMeshComponent>();
    catMesh->SetMesh(GetAssetManager()->GetMesh("cat.fbx"));
    auto catAnim = catMesh->GetAnimPlayer();
    catAnim->Play(0);
    
    
    
    
    // 建物
    auto towerActor = CreateActor<toy::Actor>();
    auto towerMesh = towerActor->CreateComponent<toy::MeshComponent>();
    towerMesh->SetMesh(GetAssetManager()->GetMesh("house.x"));
    
    auto towerCollider = towerActor->CreateComponent<toy::ColliderComponent>();
    towerCollider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("house.x")->GetVertexArray());
    towerCollider->SetDisp(true);
    towerCollider->SetFlags(toy::C_WALL | toy::C_GROUND | toy::C_FOOT);
    towerCollider->GetBoundingVolume()->AdjustBoundingBox(Vector3(0,0,0), Vector3(0.9, 0.9, 0.9));
    towerActor->SetPosition(Vector3(-60, 0, 15));
    towerActor->SetScale(0.003f);
    q = Quaternion(Vector3::UnitY, Math::ToRadians(150));
    towerActor->SetRotation(q);
    towerActor->CreateComponent<toy::GravityComponent>();
    

    // レンガ
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            auto brickActor = CreateActor<toy::Actor>();
            auto brickMesh = brickActor->CreateComponent<toy::MeshComponent>();
            brickMesh->SetMesh(GetAssetManager()->GetMesh("brick.x"));
            
            brickActor->SetPosition(Vector3(-100 + 20*j/2 + 10*i*2, 20, -20 + 5*j*2));
            brickActor->SetScale(5.f);
            auto brickCollider = brickActor->CreateComponent<toy::ColliderComponent>();
            brickCollider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("brick.x")->GetVertexArray());
            brickCollider->SetFlags(toy::C_GROUND);// | toy::C_WALL | toy::C_FOOT);
            //brickActor->CreateComponent<toy::GravityComponent>();
        }
    }
 
    for (int i = 0; i < 10; i++)
    {
        auto brickActor = CreateActor<toy::Actor>();
        auto brickMesh = brickActor->CreateComponent<toy::MeshComponent>();
        brickMesh->SetMesh(GetAssetManager()->GetMesh("brick.x"));
        
        brickActor->SetPosition(Vector3(0, i*10, -50 + i*5));
        brickActor->SetScale(5.f);
        auto brickCollider = brickActor->CreateComponent<toy::ColliderComponent>();
        brickCollider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("brick.x")->GetVertexArray());
        brickCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
        brickActor->CreateComponent<toy::GravityComponent>();
    }
    auto brickActor = CreateActor<toy::Actor>();
    auto brickMesh = brickActor->CreateComponent<toy::MeshComponent>();
    brickMesh->SetMesh(GetAssetManager()->GetMesh("brick.x"));
    
    brickActor->SetPosition(Vector3(0, -1, -50));
    brickActor->SetScale(5.f);
    auto brickCollider = brickActor->CreateComponent<toy::ColliderComponent>();
    brickCollider->GetBoundingVolume()->ComputeBoundingVolume(GetAssetManager()->GetMesh("brick.x")->GetVertexArray());
    brickCollider->SetFlags(toy::C_GROUND | toy::C_WALL );





    // 地面
    auto b = CreateActor<toy::Actor>();
    auto g = b->CreateComponent<toy::MeshComponent>(false);
    g->SetMesh(GetAssetManager()->GetMesh("ground2.x"));
    b->SetPosition(Vector3(0,0,0));
    b->SetScale(1);
    g->SetToonRender(false, 1.0);
    g->SetEnableShadow(false);
    
    auto groundMesh = GetAssetManager()->GetMesh("ground2.x");
    auto va = groundMesh->GetVertexArray();
    auto vaList = groundMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        b->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(b->GetWorldTransform());
        GetPhysWorld()->SetGroundPolygons(polys); // or 統合してまとめる
    }
    

    auto stanMove = stanActor->CreateComponent<toy::FollowMoveComponent>();
    stanMove->SetTarget(hero);
    stanMove->SetFollowSpeed(1);
    stanActor->CreateComponent<toy::GravityComponent>();



    // スカイドーム
    auto skyActor = CreateActor<toy::Actor>();
    auto dome = skyActor->CreateComponent<toy::WeatherDomeComponent>();
    // オーバーレイ
    auto overlay = skyActor->CreateComponent<toy::WeatherOverlayComponent>();
    
    mWeather = std::make_unique<toy::WeatherManager>();
    mWeather->SetWeatherDome(dome);
    mWeather->SetWeatherOverlay(overlay);
    skyActor->SetPosition(Vector3(0.f, -0.f, 0.f));
    mWeather->ChangeWeather(toy::WeatherType::CLEAR);
    
    
    // BGM
    GetSoundMixer()->LoadBGM("MusMus-BGM-112.mp3");
    GetSoundMixer()->PlayBGM();
    GetSoundMixer()->SetVolume(0.1);
    
    
    
    // フォント
    auto fnt = GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 24);
    // テキスト用 Actor を作成
    auto uiActor = CreateActor<toy::Actor>();
    //uiActor->SetPosition(Vector3(600.0f, 360.0f, 0.0f)); // 2Dスクリーン座標として扱う
    uiActor->SetPosition(Vector3(10.0f, 10.0f, 0.0f)); // 2Dスクリーン座標として扱う

    auto textComp = uiActor->CreateComponent<toy::TextSpriteComponent>();
    textComp->SetFont(fnt);
    textComp->SetFormat("");
    textComp->SetColor(Vector3(1.0f, 1.0f, 0.0f)); // 黄
    mTextComp = textComp;
}


void GameRPG::UpdateGame(float deltaTime)
{
    mWeather->Update(deltaTime);
    auto h = GetTimeOfDaySystem()->GetHour();
    auto m = GetTimeOfDaySystem()->GetMinute();
    mTextComp->SetFormat("<< : <<", h, m);
}

void GameRPG::ShutdownGame()
{
    GetSoundMixer()->StopBGM();
    
}
