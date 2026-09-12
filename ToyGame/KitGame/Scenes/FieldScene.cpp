#include "FieldScene.h"
#include "SnowScene.h"
#include "ToyLib.h"
#include "../Actors/Player.h"
#include "../Actors/Noriko.h"



FieldScene::FieldScene()
{

}

FieldScene::~FieldScene() = default;

void FieldScene::InitScene()
{
    DefineEnvironment();
    DefineWorld();
    DefineUI();
}

//-----------------------------------------------------------------------------
// DefineEnvironment — ポストエフェクト / 時間帯 / BGM
//-----------------------------------------------------------------------------
void FieldScene::DefineEnvironment()
{
    toy::PostEffectDesc effectDesc;
    effectDesc.type      = toy::PostEffectType::None;
    effectDesc.intensity = 1.0f;
    effectDesc.paperTex  = GetApp()->GetAssetManager()->GetTexture("Texture/camvas.jpg");
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);

    GetApp()->GetTimeOfDaySystem()->SetTimeScale(1000.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(10.0f, 0.0f);

    GetApp()->GetSoundMixer()->LoadBGM("BGM/MusMus-BGM-112.ogg");
    GetApp()->GetSoundMixer()->PlayBGM();
    GetApp()->GetSoundMixer()->SetBgmVolume(0.5f);
    GetApp()->GetSoundMixer()->SetMasterVolume(0.8f);
}

//-----------------------------------------------------------------------------
// DefineWorld — 地形/プロップ（InitField）+ プレイヤー + エネミー
//-----------------------------------------------------------------------------
void FieldScene::DefineWorld()
{
    InitField();

    mPlayer = MakePlayer(GetApp());

    // Noriko（プレイヤーが視界に入ったら逃げる。Creature + FleeBehavior の Agent）
    for (int i = 0; i < 10; ++i)
    {
        Vector3 pos(-30.0f + static_cast<float>(i * 10), 3.0f, 10.0f);
        mMonsters.push_back(MakeNoriko(GetApp(), pos, &mPlayer->GetBody()));
    }
}

//-----------------------------------------------------------------------------
// DefineUI — 時刻表示
//-----------------------------------------------------------------------------
void FieldScene::DefineUI()
{
    auto fnt      = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 24);
    auto uiActor  = CreateActor<toy::Actor>();
    uiActor->SetPosition(Vector3(1100.0f, 10.0f, 0.0f)); // 2Dスクリーン座標として扱う

    auto text = uiActor->CreateComponent<toy::TextSpriteComponent>();
    text->SetFont(fnt);
    text->SetFormat("");
    text->SetColor(Vector3(1.0f, 1.0f, 0.0f)); // 黄
    mTextComp = text;
}

void FieldScene::ProcessInput(const struct toy::InputState &input)
{
    if (mPlayer)
    {
        mPlayer->ProcessInput(input);
    }

    if (input.IsButtonPressed(toy::GameButton::Start))
    {
        RequestChange(std::make_unique<SnowScene>());
    }
}


void FieldScene::Update(float deltaTime)
{
    if (mWeather)
    {
        mWeather->Update(deltaTime);
    }

    if (mPlayer)
    {
        mPlayer->Update(deltaTime);
    }

    for (auto& monster : mMonsters)
    {
        monster->Update(deltaTime);
    }

    auto h = GetApp()->GetTimeOfDaySystem()->GetHour();
    auto m = GetApp()->GetTimeOfDaySystem()->GetMinute();
    (void)m;
        
    mTextComp->SetFormat("時刻 {:02} : {:02}  \n", h, 0);
    
    toy::DebugDraw::Clear();
    toy::DebugDraw::Ray(Vector3(0,0,-100), Vector3::UnitZ, 200.0f);
    toy::DebugDraw::Ray(Vector3(0,5,-100), Vector3::UnitZ, 200.0f);
    toy::DebugDraw::Ray(Vector3(-100,0,0), Vector3::UnitX, 200.0f);
    toy::DebugDraw::Ray(Vector3(-100,5,0), Vector3::UnitX, 200.0f);

    
    Vector3 pos = mPlayer->GetBody().GetPosition();
    toy::DebugDraw::Sphere(pos, 5.0f, 32);
    //toy::DebugDraw::Box(min, max);
}

void FieldScene::InitField()
{
    DeployGround();
    DeploySky();
    DeployFire(Vector3::Zero);

    // レンガ
    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            DeployBrick(Vector3(-80 + 15 * j / 2 + 10 * i * 1.5, 20, 20 + 5 * j * 1.5));
        }
    }

    for (int i = 0; i < 12; ++i)
    {
        DeployBrick(Vector3(0, -3 + i * 2, -20 + i * 4));

    }

    // 木（ビルボード）
    auto treeActor = CreateActor<toy::Actor>();
    treeActor->SetPosition(Vector3(20.0f, 4.5f, -50.0f));
    treeActor->SetScale(0.02);
    auto treeBillboard = treeActor->CreateComponent<toy::BillboardComponent>();
    treeBillboard->SetTexture(GetApp()->GetAssetManager()->GetTexture("Field/tree.png"));
    treeBillboard->SetVisible(true);
    treeActor->CreateComponent<toy::GravityComponent>();
    auto treeCollider = treeActor->CreateComponent<toy::ColliderComponent>();
    treeCollider->GetBoundingVolume()->ComputeBoundingVolume(Vector3(-100, -256, -4), Vector3(100, 200, 4));
    treeCollider->SetFlags(toy::C_WALL | toy::C_FOOT);

    {
        // 鏡を出す
        auto mirrorActor = CreateActor<toy::Actor>();
        mirrorActor->SetPosition(Vector3(20.0f, 0.0f, 15.0f));
        mirrorActor->SetScale(1.0f);
        Quaternion q = Quaternion(Vector3::UnitY, Math::ToRadians(45.0f));
        mirrorActor->SetRotation(q);
        auto capture = mirrorActor->CreateComponent<toy::SceneCaptureComponent>();
        capture->Init({.width=512, .height=512, .updateHz = 30});
        capture->SetCaptureMode(toy::CaptureMode::Mirror);

        auto mirrorComp = mirrorActor->CreateComponent<toy::RenderSurfaceComponent>();
        mirrorComp->SetTexture(capture->GetColorTexture());
        mirrorComp->SetScale(10.0f, 10.0f);
        capture->SetSurfaceInfo({ .scWidth=10.f, .scHeight=10.0f});
        mirrorComp->SetFlip(true, false);
        mirrorComp->SetSurfaceMode(toy::SurfaceMode::Mirror);
    }
    {
        // 水面を出す
        auto waterActor = CreateActor<toy::Actor>();
        waterActor->SetPosition(Vector3(20.0f, -4.0f, 40.0f));
        waterActor->SetScale(1.0f);
        Quaternion q = Quaternion(Vector3::UnitX, Math::ToRadians(90.0f));
        waterActor->SetRotation(q);
        auto waterCapture = waterActor->CreateComponent<toy::SceneCaptureComponent>();
        waterCapture->Init({ .width = 512, .height = 512, .updateHz =  30});
        waterCapture->SetCaptureMode(toy::CaptureMode::Water);

        auto waterComp = waterActor->CreateComponent<toy::RenderSurfaceComponent>();
        waterComp->SetTexture(waterCapture->GetColorTexture());
        waterComp->SetScale(40.0f, 40.0f);
        waterCapture->SetSurfaceInfo({ .scWidth = 20.f, .scHeight = 20.0f });
        waterComp->SetFlip(true, false);
        waterComp->SetSurfaceMode(toy::SurfaceMode::Water);
        waterComp->SetOpacity(0.7f);
    }

}

void FieldScene::DeploySky()
{
    // スカイドーム
    auto skyActor = CreateActor<toy::Actor>();
    auto dome = skyActor->CreateComponent<toy::WeatherDomeComponent>();
    // オーバーレイ
    auto overlay = skyActor->CreateComponent<toy::WeatherOverlayComponent>();

    mWeather = std::make_unique<toy::WeatherManager>();
    mWeather->SetWeatherDome(dome);
    mWeather->SetWeatherOverlay(overlay);
    mWeather->ChangeWeather(toy::WeatherType::CLEAR);
}


void FieldScene::DeployGround()
{
    // 地面
    auto actor = CreateActor<toy::Actor>();
    auto meshComp = actor->CreateComponent<toy::MeshComponent>();
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Field/ground.glb"));
    actor->SetPosition(Vector3(0,0,0));
    actor->SetScale(1);
    meshComp->SetToonRender(false);
    meshComp->SetEnableShadow(false);
    
    auto groundMesh = GetApp()->GetAssetManager()->GetMesh("Field/ground.glb");
    auto va = groundMesh->GetVertexArray();
    auto vaList = groundMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        actor->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(actor->GetWorldTransform());
        GetApp()->GetPhysWorld()->SetGroundPolygons(polys);
    }
}

void FieldScene::DeployBrick(Vector3 pos)
{
    toy::kit::StaticObjectDesc desc;
    desc.model         = "Field/brick.glb";
    desc.toonRender    = false;
    desc.actorScale    = 4.0f;
    desc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_CEILING;

    auto brick = std::make_unique<toy::kit::StaticObject>(GetApp(), desc);
    brick->SetPosition(pos);
    mStaticObjects.push_back(std::move(brick));
}

void FieldScene::DeployFire(Vector3 pos)
{
    // 焚き火
    auto fireActor = CreateActor<toy::Actor>();
    auto fireMesh = fireActor->CreateComponent<toy::MeshComponent>();
    fireMesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("Field/campfire.glb"));
    
    fireActor->SetPosition(Vector3(-8, 0, -30));
    fireActor->SetScale(0.02f);
    auto fireCollider = fireActor->CreateComponent<toy::ColliderComponent>();
    fireCollider->GetBoundingVolume()->ComputeBoundingVolume(GetApp()->GetAssetManager()->GetMesh("Field/campfire.glb")->GetVertexArray());
    fireCollider->SetEnabled(true);
    fireCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    fireActor->CreateComponent<toy::GravityComponent>();
    
    auto fireSound = fireActor->CreateComponent<toy::SoundComponent>();
    fireSound->SetSound("Field/fire.wav");
    fireSound->SetLoop(true);
    fireSound->SetVolume(1.0f);
    fireSound->Enable3DSound(true);
    fireSound->Play();
    
    
    // ライト
    auto fireLight = fireActor->CreateComponent<toy::PointLightComponent>();
    fireLight->SetColor(Vector3(1.0f, 0.5f, 0.0f));
    
    
    {

        // Actor
        auto a = CreateActor<toy::Actor>();
        a->SetPosition(fireActor->GetPosition());
        // Component
        auto* p = a->CreateComponent<toy::ParticleComponent>();

        p->SetTexture(
            GetApp()->GetAssetManager()->GetTexture("Field/fire.png"));

        //==============================
        // Desc で全設定
        //==============================
        toy::ParticleDesc desc;
        
        // --- 基本 ---
        desc.maxParticles   = 10;        // 旧 num
        desc.particleLife   = 0.6f;       // 旧 partLife
        desc.size           = 5.5f;       // 旧 size
        desc.mode           = toy::ParticleMode::Smoke;
        
        // --- エミッタ ---
        desc.emitterOffset  = Vector3(0.0f, -1.0f, 0.0f); // Actor ローカル
        desc.spawnRatePerSec = 30.0f;     // 1秒あたりの発生数
        desc.spawnRampSec    = 0.6f;      // 立ち上がり時間
        
        // --- 見た目 ---
        desc.additiveBlend  = true;       // SetAddBlend(true) 相当
        
        // --- 物理 ---
        desc.gravity = 0.0f;              // Smoke
        desc.lift    = 2.0f;              // 上昇力
        desc.spread  = 2.0f;              // 拡散速度
        
        // --- コンポーネント寿命 ---
        // 0 = 無限（焚き火用）
        desc.componentLife = 0.0f;
        
        // --- 初期分散（塊回避） ---
        desc.warmStart = true;
        
        
        p->Init(desc);
        //p->InitFromFile("ToyGame/Settings/Fire.json");
        p->Start();
    
    }
    {
        auto a = CreateActor<toy::Actor>();
        a->SetPosition(Vector3(8, 4, -30));
        a->SetScale(0.2f);
        auto p = a->CreateComponent<toy::ParticleComponent>();
        p->SetTexture(GetApp()->GetAssetManager()->GetTexture("Field/parts.jpg"));
        auto l = a->CreateComponent<toy::PointLightComponent>();
        l->SetColor(Vector3(0.5f, 0.5f, 1.0f));
        
        
        //==============================
        // Desc で全設定
        //==============================
        toy::ParticleDesc desc;
        
        // --- 基本 ---
        desc.maxParticles   = 60;        // 旧 num
        desc.particleLife   = 1.5f;       // 旧 partLife
        desc.size           = 2.0f;       // 旧 size
        desc.mode           = toy::ParticleMode::Water;
        
        // --- エミッタ ---
        desc.emitterOffset  = Vector3(0.0f, -1.0f, 0.0f); // Actor ローカル
        desc.spawnRatePerSec = 5.0f;     // 1秒あたりの発生数
        desc.spawnRampSec    = 1.6f;      // 立ち上がり時間
        
        // --- 見た目 ---
        desc.additiveBlend  = true;       // SetAddBlend(true) 相当
        
        // --- 物理 ---
        desc.gravity = 6.0f;              // Smoke
        desc.lift    = 0.0f;              // 上昇力
        desc.spread  = 1.0f;              // 拡散速度
        
        // --- コンポーネント寿命 ---
        // 0 = 無限（焚き火用）
        desc.componentLife = 0.0f;
        
        // --- 初期分散（塊回避） ---
        desc.warmStart = true;
        
        
        p->Init(desc);
        p->Start();
    }

}
