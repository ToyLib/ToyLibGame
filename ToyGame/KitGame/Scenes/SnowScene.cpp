#include "SnowScene.h"
#include "FieldScene.h"
#include "ToyLib.h"
#include "../Actors/PlayerActor.h"
#include "../Actors/RPGCharacter.h"
#include "../Actors/EnemyActor.h"



SnowScene::SnowScene()
{
    
}

void SnowScene::InitScene()

{
    toy::PostEffectDesc effectDesc;
    effectDesc.type = toy::PostEffectType::FeilyLand;
    effectDesc.intensity = 1.0f;
    effectDesc.paperTex = GetApp()->GetAssetManager()->GetTexture("Texture/camvas.jpg");
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);
    
    
    // 時間の設定
    GetApp()->GetTimeOfDaySystem()->SetTimeScale(000.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(22.0f, 0.0f);

   
    
    // BGM
    GetApp()->GetSoundMixer()->LoadBGM("BGM/MusMus-BGM-112.mp3");
    GetApp()->GetSoundMixer()->PlayBGM();
    GetApp()->GetSoundMixer()->SetBgmVolume(0.1f);
    GetApp()->GetSoundMixer()->SetMasterVolume(0.0f);

    InitField();


    mPlayerActor = CreateActor<PlayerActor>();
    
    // エネミー
    for (int i = 0; i < 10; ++i)
    {
        auto enemy = CreateActor<EnemyActor>();
        enemy->SetPosition(Vector3(-30.0f + static_cast<float>(i * 10), 3.0f, 10.0f));
    }
 
    
    
    // フォント
    auto fnt = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 24);
    // テキスト用 Actor を作成
    auto uiActor = CreateActor<toy::Actor>();
    //uiActor->SetPosition(Vector3(600.0f, 360.0f, 0.0f)); // 2Dスクリーン座標として扱う
    uiActor->SetPosition(Vector3(1100.0f, 10.0f, 0.0f)); // 2Dスクリーン座標として扱う

    auto text = uiActor->CreateComponent<toy::TextSpriteComponent>();
    text->SetFont(fnt);
    text->SetFormat("");
    text->SetColor(Vector3(1.0f, 1.0f, 0.0f)); // 黄
    mTextComp = text;
    

    {
        // 鏡を出す
        auto mirrorActor = CreateActor<toy::Actor>();
        mirrorActor->SetPosition(Vector3(20.0f, 0.0f, 15.0f));
        mirrorActor->SetScale(1.0f);
        Quaternion q = Quaternion(Vector3::UnitY, Math::ToRadians(45.0f));
        mirrorActor->SetRotation(q);
        auto capture = mirrorActor->CreateComponent<toy::SceneCaptureComponent>();
        capture->Init({ .width = 512, .height = 512 });
        capture->SetCaptureMode(toy::CaptureMode::Mirror);

        auto mirrorComp = mirrorActor->CreateComponent<toy::RenderSurfaceComponent>();
        mirrorComp->SetTexture(capture->GetColorTexture());
        mirrorComp->SetScale(10.0f, 10.0f);
        capture->SetSurfaceInfo({ .scWidth = 10.f, .scHeight = 10.0f });
        mirrorComp->SetFlip(true, true);
        mirrorComp->SetSurfaceMode(toy::SurfaceMode::Monitor);
    }
    {
        mBackCamera = CreateActor<toy::Actor>();
        mBackCamera->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        auto capture = mBackCamera->CreateComponent<toy::SceneCaptureComponent>();
        capture->Init({ .width = 320, .height = 240 });
        capture->SetCaptureMode(toy::CaptureMode::Fixed);
        
        auto b = CreateActor<toy::Actor>();
        b->SetPosition(Vector3(700, 400, 0));
        auto sp = b->CreateComponent<toy::SpriteComponent>();
        sp->SetTexture(capture->GetColorTexture());
    }

    /*
    { // テスト用スプライト
        auto a = CreateActor<toy::Actor>();
        auto sp = a->CreateComponent<toy::SpriteComponent>(1000);
        sp->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/target3.png"));
        a->SetPosition(Vector3(100.0f, 100.0f,0));
    }
    */
    /*{ // テスト用雪
        auto a = CreateActor<toy::Actor>();
        auto sn = a->CreateComponent<toy::SnowFieldComponent>();
        sn->SetTexture(GetApp()->GetAssetManager()->GetTexture("Field/snow.png"));
        sn->SetBaseScale(0.005f);
    }*/
    {
        auto a = CreateActor<toy::Actor>();
        auto snow = a->CreateComponent<toy::ParticleComponent>();
        snow->SetTexture(GetApp()->GetAssetManager()->GetTexture("Field/snow.png"));
        
        toy::ParticleDesc desc;

        desc.mode = toy::ParticleMode::SnowField;

        // ---------------------------------------------------------
        // 基本
        // ---------------------------------------------------------
        desc.maxParticles = 512;

        desc.componentLife = 0.0f;   // 常時稼働
        desc.particleLife  = 10.0f;

        desc.size = 0.22f;

        desc.spawnRatePerSec = 0.0f; // SnowField では未使用
        desc.spawnRampSec    = 0.0f;

        desc.spread  = 0.0f;         // SnowField では未使用
        desc.gravity = 0.45f;
        desc.lift    = 0.0f;

        desc.additiveBlend = false;
        desc.warmStart     = true;

        desc.emitterOffset = Vector3::Zero;

        // ---------------------------------------------------------
        // SnowField 専用
        // ---------------------------------------------------------
        desc.fieldExtent = Vector3(60.0f, 16.0f, 60.0f);
        desc.wind         = Vector3(0.12f, 0.0f, 0.05f);

        desc.followCamera = true;
        desc.respawnTop   = true;

        
        snow->Init(desc);
        snow->Start();
    }
    
    
}

void SnowScene::ProcessInput(const struct toy::InputState &input)
{
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
    
    auto h = GetApp()->GetTimeOfDaySystem()->GetHour();
    auto m = GetApp()->GetTimeOfDaySystem()->GetMinute();
    (void)m;
        
    mTextComp->SetFormat("時刻 {:02} : {:02}  \n", h, 0);
    
    toy::DebugDraw::Clear();
    toy::DebugDraw::Ray(Vector3(0,0,-100), Vector3::UnitZ, 200.0f);
    toy::DebugDraw::Ray(Vector3(0,5,-100), Vector3::UnitZ, 200.0f);
    toy::DebugDraw::Ray(Vector3(-100,0,0), Vector3::UnitX, 200.0f);
    toy::DebugDraw::Ray(Vector3(-100,5,0), Vector3::UnitX, 200.0f);

    
    Vector3 pos = mPlayerActor->GetPosition();
    toy::DebugDraw::Sphere(pos, 5.0f, 32);
    //toy::DebugDraw::Box(min, max);
    
    
    mBackCamera->SetPosition(mPlayerActor->GetPosition() + Vector3(0.0f, 3.0f, 0.0f));
    auto mat = mPlayerActor->GetWorldTransform();
    mBackCamera->SetRotation(Quaternion::CreateFromMatrix(mat));
    
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
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Field_snow/snow_ground.x"));
    actor->SetPosition(Vector3(0,0,0));
    actor->SetScale(1);
    meshComp->SetToonRender(false);
    meshComp->SetEnableShadow(false);
    
    auto groundMesh = GetApp()->GetAssetManager()->GetMesh("Field_snow/snow_ground.x");
    auto va = groundMesh->GetVertexArray();
    auto vaList = groundMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        actor->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(actor->GetWorldTransform());
        GetApp()->GetPhysWorld()->SetGroundPolygons(polys);
    }
}

void SnowScene::DeployBrick(Vector3 pos)
{


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
