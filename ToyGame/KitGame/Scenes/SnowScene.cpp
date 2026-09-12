#include "SnowScene.h"
#include "FieldScene.h"
#include "ToyLib.h"
#include "../Actors/Player.h"
#include "../Actors/Noriko.h"



SnowScene::SnowScene()
{

}

SnowScene::~SnowScene() = default;

//-----------------------------------------------------------------------------
// DefineEnvironment — ポストエフェクト / 時間帯 / BGM
//-----------------------------------------------------------------------------
void SnowScene::DefineEnvironment()
{
    toy::PostEffectDesc effectDesc;
    effectDesc.type      = toy::PostEffectType::FeilyLand;
    effectDesc.intensity = 1.0f;
    effectDesc.paperTex  = GetApp()->GetAssetManager()->GetTexture("Texture/camvas.jpg");
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);

    GetApp()->GetTimeOfDaySystem()->SetTimeScale(3000.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(22.0f, 0.0f);

    GetApp()->GetSoundMixer()->LoadBGM("BGM/MusMus-BGM-112.ogg");
    GetApp()->GetSoundMixer()->PlayBGM();
    GetApp()->GetSoundMixer()->SetBgmVolume(0.5f);
    GetApp()->GetSoundMixer()->SetMasterVolume(0.7f);
}

//-----------------------------------------------------------------------------
// DefineWorld — 地形/プロップ（InitField）+ 鏡 + 主人公視点カメラ + 雪
//-----------------------------------------------------------------------------
void SnowScene::DefineWorld()
{
    InitField();

    // 鏡を出す
    auto mirrorActor = CreateActor<toy::Actor>();
    mirrorActor->SetPosition(Vector3(20.0f, 0.0f, 15.0f));
    mirrorActor->SetScale(1.0f);
    mirrorActor->SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(45.0f)));
    auto capture = mirrorActor->CreateComponent<toy::SceneCaptureComponent>();
    capture->Init({ .width = 512, .height = 512 });
    capture->SetCaptureMode(toy::CaptureMode::Mirror);

    auto mirrorComp = mirrorActor->CreateComponent<toy::RenderSurfaceComponent>();
    mirrorComp->SetTexture(capture->GetColorTexture());
    mirrorComp->SetScale(10.0f, 10.0f);
    capture->SetSurfaceInfo({ .scWidth = 10.f, .scHeight = 10.0f });
    mirrorComp->SetFlip(true, false);
    mirrorComp->SetSurfaceMode(toy::SurfaceMode::Monitor);

    // 主人公視点（画面右上のミニカメラ）
    mPlyCamera = CreateActor<toy::Actor>();
    mPlyCamera->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    auto plyCapture = mPlyCamera->CreateComponent<toy::SceneCaptureComponent>();
    plyCapture->Init({ .width = 320, .height = 240 });
    plyCapture->SetCaptureMode(toy::CaptureMode::Fixed);
    plyCapture->SetSurfaceInfo({ .scWidth = 10.f, .scHeight = 10.0f });

    auto plyCameraDisplay = CreateActor<toy::Actor>();
    plyCameraDisplay->SetPosition(Vector3(800, 400, 0));
    auto plyCameraSprite = plyCameraDisplay->CreateComponent<toy::SpriteComponent>();
    plyCameraSprite->SetTexture(plyCapture->GetColorTexture());

    // 雪
    auto snowActor = CreateActor<toy::Actor>();
    auto snow = snowActor->CreateComponent<toy::ParticleComponent>();
    snow->SetTexture(GetApp()->GetAssetManager()->GetTexture("Field/snow.png"));

    toy::ParticleDesc snowDesc;
    snowDesc.mode = toy::ParticleMode::SnowField;

    snowDesc.maxParticles  = 512;
    snowDesc.componentLife = 0.0f;   // 常時稼働
    snowDesc.particleLife  = 10.0f;
    snowDesc.size          = 0.22f;

    snowDesc.spawnRatePerSec = 0.0f; // SnowField では未使用
    snowDesc.spawnRampSec    = 0.0f;
    snowDesc.spread          = 0.0f; // SnowField では未使用
    snowDesc.gravity         = 0.45f;
    snowDesc.lift            = 0.0f;

    snowDesc.additiveBlend = false;
    snowDesc.warmStart     = true;
    snowDesc.emitterOffset = Vector3::Zero;

    // SnowField 専用
    snowDesc.fieldExtent = Vector3(60.0f, 16.0f, 60.0f);
    snowDesc.wind        = Vector3(0.12f, 0.0f, 0.05f);
    snowDesc.followCamera = true;
    snowDesc.respawnTop   = true;

    snow->Init(snowDesc);
    snow->Start();
}

//-----------------------------------------------------------------------------
// SpawnCharacters — プレイヤー + エネミー
//-----------------------------------------------------------------------------
void SnowScene::SpawnCharacters()
{
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
void SnowScene::DefineUI()
{
    auto fnt     = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 24);
    auto uiActor = CreateActor<toy::Actor>();
    uiActor->SetPosition(Vector3(1100.0f, 10.0f, 0.0f)); // 2Dスクリーン座標として扱う

    auto text = uiActor->CreateComponent<toy::TextSpriteComponent>();
    text->SetFont(fnt);
    text->SetFormat("");
    text->SetColor(Vector3(1.0f, 1.0f, 0.0f)); // 黄
    mTextComp = text;
}

void SnowScene::ProcessInput(const struct toy::InputState &input)
{
    if (mPlayer)
    {
        mPlayer->ProcessInput(input);
    }

    if (input.IsButtonPressed(toy::GameButton::Start))
    {
        RequestChange(std::make_unique<FieldScene>());
    }
}

void SnowScene::Update(float deltaTime)
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


    mPlyCamera->SetPosition(mPlayer->GetBody().GetPosition() + Vector3(0.0f, 3.0f, 0.0f));
    auto mat = mPlayer->GetBody().GetWorldTransform();
    mat *= Matrix4::CreateRotationY(Math::ToRadians(180.0f));
    mPlyCamera->SetRotation(Quaternion::CreateFromMatrix(mat));
    
   }

void SnowScene::InitField()
{
    DeployGround();
    DeploySky();
    DeployFire(Vector3::Zero);

}

void SnowScene::DeploySky()
{
    // スカイドーム
    auto skyActor = CreateActor<toy::Actor>();
    auto dome = skyActor->CreateComponent<toy::WeatherDomeComponent>();
    // オーバーレイ
    auto overlay = skyActor->CreateComponent<toy::WeatherOverlayComponent>();
    
    mWeather = std::make_unique<toy::WeatherManager>();
    mWeather->SetWeatherDome(dome);
    mWeather->SetWeatherOverlay(overlay);
    mWeather->ChangeWeather(toy::WeatherType::SNOW);
}


void SnowScene::DeployGround()
{
    // 地面
    auto actor = CreateActor<toy::Actor>();
    auto meshComp = actor->CreateComponent<toy::MeshComponent>();
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Field_snow/snow_ground.glb"));
    actor->SetPosition(Vector3(0,0,0));
    actor->SetScale(1);
    meshComp->SetToonRender(false);
    meshComp->SetEnableShadow(false);
    
    auto groundMesh = GetApp()->GetAssetManager()->GetMesh("Field_snow/snow_ground.glb");
    auto va = groundMesh->GetVertexArray();
    auto vaList = groundMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        actor->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(actor->GetWorldTransform());
        GetApp()->GetPhysWorld()->SetGroundPolygons(polys);
    }
}

void SnowScene::DeployFire(Vector3 pos)
{

    
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
