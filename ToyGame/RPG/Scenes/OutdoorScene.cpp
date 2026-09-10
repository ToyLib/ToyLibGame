#include "OutdoorScene.h"
#include "ToyLib.h"
#include "../Actors/Hero.h"
#include "../Actors/Wolf.h"
#include "../Actors/Shiro.h"
#include "../Actors/MagicBolt.h"
#include "../Actors/HealBurst.h"
#include "../Actors/IslandActor.h"

#include <vector>

OutdoorScene::OutdoorScene()  = default;
OutdoorScene::~OutdoorScene() = default;

//=============================================================================
// InitScene
//=============================================================================

void OutdoorScene::InitScene()
{
    // ポストエフェクト
    toy::PostEffectDesc effectDesc;
    effectDesc.type      = toy::PostEffectType::FeilyLand;
    effectDesc.intensity = 1.0f;
    effectDesc.paperTex  = GetApp()->GetAssetManager()->GetTexture("paper_tex.jpg");
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);

    // 時間帯
    GetApp()->GetTimeOfDaySystem()->SetTimeScale(10000.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(12.0f, 30.0f);

    // BGM
    GetApp()->GetSoundMixer()->LoadBGM("MusMus-BGM-112.ogg");
    GetApp()->GetSoundMixer()->PlayBGM();
    GetApp()->GetSoundMixer()->SetBgmVolume(0.5f);
    GetApp()->GetSoundMixer()->SetMasterVolume(0.8f);

    InitField();

    // プレイヤー（新方針: Humanoid Prefab を内包する Game Logic）
    mHero = std::make_unique<Hero>(GetApp());

    // Hero が発動した魔法/回復を Scene 側でエフェクト化する
    // （Hero 自身はエフェクトの生成・寿命管理を持たない）
    mHero->OnCastMagic().Connect(
        [this](const CastMagicEvent& e)
        {
            mMagicBolts.push_back(std::make_unique<MagicBolt>(GetApp(), e.position, e.forward));
        });
    mHero->OnCastHeal().Connect(
        [this](const CastHealEvent& e)
        {
            mHealBursts.push_back(std::make_unique<HealBurst>(GetApp(), e.position));
        });

    // Wolf x5（プレイヤーをターゲットに。新方針: Humanoid Prefab を内包する Game Logic）
    for (int i = 0; i < 5; ++i)
    {
        auto wolf = std::make_unique<Wolf>(GetApp());
        wolf->SetPosition(Vector3(-20.0f + i * 10.0f, 3.0f, -20.0f));
        wolf->SetTarget(&mHero->GetBody());
        mWolves.push_back(std::move(wolf));
    }

    // Shiro（焚き火の向かい側。新方針: Humanoid Prefab を内包する Game Logic）
    mShiro = std::make_unique<Shiro>(GetApp());
    mShiro->SetPosition(Vector3(0.0f, 0.0f, -25.0f));
    mShiro->SetTarget(&mHero->GetBody());

    // Hero の位置を毎フレーム反映するマーカー Actor。
    // Hero 自体は toy::Actor を持たないため、FollowMoveComponent など
    // 「本物の Actor」をターゲットに要求するレガシー系のためだけに用意する。
    mHeroMarker = CreateActor<toy::Actor>();
    mHeroMarker->SetPosition(mHero->GetPosition());

    // Stan（プレイヤー追従）
    auto* stan = CreateActor<toy::Actor>();
    stan->SetPosition(Vector3(-3.0f, 0.0f, 10.0f));
    stan->SetScale(0.5f);
    stan->SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(-30.0f)));

    auto* stanMesh = stan->CreateComponent<toy::SkeletalMeshComponent>();
    stanMesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("stan.gltf", true));
    stanMesh->SetToonRender(true);

    auto* stanColl = stan->CreateComponent<toy::ColliderComponent>();
    stanColl->GetBoundingVolume()->ComputeBoundingVolume(
        GetApp()->GetAssetManager()->GetMesh("stan.gltf")->GetVertexArray());
    stanColl->GetBoundingVolume()->AdjustBoundingBox(Vector3::Zero, Vector3(0.5f, 1.0f, 0.6f));
    stanColl->GetBoundingVolume()->CreateVArray();
    stanColl->SetFlags(toy::C_WALL | toy::C_ENEMY_TEAM | toy::C_HURTBOX | toy::C_FOOT | toy::C_GROUND);
    stanColl->SetEnabled(true);

    auto* stanMove = stan->CreateComponent<toy::FollowMoveComponent>();
    stanMove->SetTarget(mHeroMarker);
    stanMove->SetFollowSpeed(10.0f);

    stan->CreateComponent<toy::GravityComponent>();

    // ヘルスバー
    auto* hbActor = CreateActor<toy::Actor>();
    hbActor->SetPosition(Vector3(0.0f, 680.0f, 0.0f));
    auto* hbSprite = hbActor->CreateComponent<toy::SpriteComponent>(100, toy::VisualLayer::UI);
    hbSprite->SetTexture(GetApp()->GetAssetManager()->GetTexture("HealthBar.png"));
    hbSprite->SetVisible(true);

    // 時刻テキスト
    auto  fnt     = GetApp()->GetAssetManager()->GetFont("rounded-mplus-1c-bold.ttf", 24);
    auto* uiActor = CreateActor<toy::Actor>();
    uiActor->SetPosition(Vector3(1100.0f, 10.0f, 0.0f));
    auto* text = uiActor->CreateComponent<toy::TextSpriteComponent>();
    text->SetFont(fnt);
    text->SetFormat("");
    text->SetColor(Vector3(1.0f, 1.0f, 0.0f));
    mTextComp = text;
}

//=============================================================================
// Update
//=============================================================================

void OutdoorScene::Update(float deltaTime)
{
    if (mWeather)
    {
        mWeather->Update(deltaTime);
    }

    if (mHero)
    {
        mHero->Update(deltaTime);

        if (mHeroMarker)
        {
            mHeroMarker->SetPosition(mHero->GetPosition());
        }
    }

    if (mShiro)
    {
        mShiro->Update(deltaTime);
    }

    for (auto& wolf : mWolves)
    {
        wolf->Update(deltaTime);
    }

    // 一時エフェクト（魔法/回復）の更新 + 寿命切れの回収
    for (auto& bolt : mMagicBolts)
    {
        bolt->Update(deltaTime);
    }
    std::erase_if(mMagicBolts, [](const auto& b) { return b->IsExpired(); });

    for (auto& burst : mHealBursts)
    {
        burst->Update(deltaTime);
    }
    std::erase_if(mHealBursts, [](const auto& b) { return b->IsExpired(); });

    // 時刻表示
    if (mTextComp)
    {
        auto h = GetApp()->GetTimeOfDaySystem()->GetHour();
        auto m = GetApp()->GetTimeOfDaySystem()->GetMinute();
        mTextComp->SetFormat("時刻 {:02} : {:02}  \n", h, m);
    }
}

//=============================================================================
// ProcessInput
//=============================================================================

void OutdoorScene::ProcessInput(const toy::InputState& input)
{
    if (mHero)
    {
        mHero->ProcessInput(input);
    }

    if (input.Keyboard.GetKeyState(SDL_SCANCODE_F6) == toy::EPressed)
    {
        bool s = GetApp()->GetRenderer()->GetEnableShadow();
        GetApp()->GetRenderer()->SetEnableShadow(!s);
    }
}

//=============================================================================
// UnloadScene
//=============================================================================

void OutdoorScene::UnloadScene()
{
    GetApp()->GetSoundMixer()->StopBGM();
}

//=============================================================================
// InitField — 地面 / 空 / 焚き火 / レンガ / 島 / 家 / 木 / 鏡
//=============================================================================

void OutdoorScene::InitField()
{
    DeployGround();
    DeploySky();

    // 焚き火
    DeployFire(Vector3::Zero);

    // レンガ
    for (int i = 0; i < 6; ++i)
    {
        DeployBrick(Vector3(0.0f, -1.0f + i * 5.0f, -15.0f + i * 5.0f));
    }

    // 島（レンガ状配置）
    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            DeployIsland(Vector3(-100.0f + 20.0f * j / 2.0f + 10.0f * i * 2.0f,
                                 20.0f,
                                 20.0f + 5.0f * j * 2.0f));
        }
    }

    // 家
    DeployHouse(Vector3(-60.0f, 0.0f, 15.0f));

    // 木
    DeployTree(Vector3(20.0f, 4.5f, 0.0f));

    // 鏡
    DeployMirror(Vector3(-20.0f, 0.0f, 15.0f));

    // IslandActor
    CreateActor<IslandActor>();
}

void OutdoorScene::DeployGround()
{
    auto* b = CreateActor<toy::Actor>();
    auto* g = b->CreateComponent<toy::MeshComponent>(false);
    g->SetMesh(GetApp()->GetAssetManager()->GetMesh("ground2.x"));
    b->SetPosition(Vector3::Zero);
    b->SetScale(1.0f);
    g->SetToonRender(false);
    g->SetEnableShadow(false);

    auto groundMesh = GetApp()->GetAssetManager()->GetMesh("ground2.x");
    for (auto& va : groundMesh->GetVertexArray())
    {
        b->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(b->GetWorldTransform());
        GetApp()->GetPhysWorld()->SetGroundPolygons(polys);
    }
}

void OutdoorScene::DeploySky()
{
    auto* skyActor = CreateActor<toy::Actor>();
    auto* dome     = skyActor->CreateComponent<toy::WeatherDomeComponent>();
    auto* overlay  = skyActor->CreateComponent<toy::WeatherOverlayComponent>();

    mWeather = std::make_unique<toy::WeatherManager>();
    mWeather->SetWeatherDome(dome);
    mWeather->SetWeatherOverlay(overlay);
    mWeather->ChangeWeather(toy::WeatherType::CLEAR);
}

void OutdoorScene::DeployFire(const Vector3& pos)
{
    auto* fire = CreateActor<toy::Actor>();
    fire->SetPosition(Vector3(-8.0f, 0.0f, -30.0f));

    auto* mesh = fire->CreateComponent<toy::MeshComponent>();
    mesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("campfile.x"));
    mesh->SetLocalScale(0.03f);

    auto* coll = fire->CreateComponent<toy::ColliderComponent>();
    coll->GetBoundingVolume()->ComputeFromMeshComponent(mesh);
    coll->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    coll->SetEnabled(true);

    fire->CreateComponent<toy::GravityComponent>();

    auto* snd = fire->CreateComponent<toy::SoundComponent>();
    snd->SetSound("fire.wav");
    snd->SetLoop(true);
    snd->SetVolume(0.5f);
    snd->Enable3DSound(true);
    snd->Play();

    auto* light = fire->CreateComponent<toy::PointLightComponent>();
    light->SetColor(Vector3(1.0f, 0.5f, 0.0f));

    // パーティクル
    auto* particleActor = CreateActor<toy::Actor>();
    particleActor->SetPosition(fire->GetPosition());
    auto* particle = particleActor->CreateComponent<toy::ParticleComponent>();
    particle->SetTexture(GetApp()->GetAssetManager()->GetTexture("fire.png"));
    particle->InitFromFile("ToyGame/Settings/Fire.json");
    particle->Start();
}

void OutdoorScene::DeployBrick(const Vector3& pos)
{
    toy::kit::StaticObjectDesc desc;
    desc.model         = "brick.x";
    desc.actorScale    = 5.0f;
    desc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_CEILING;

    auto brick = std::make_unique<toy::kit::StaticObject>(GetApp(), desc);
    brick->SetPosition(pos);
    mStaticObjects.push_back(std::move(brick));
}

void OutdoorScene::DeployIsland(const Vector3& pos)
{
    toy::kit::StaticObjectDesc desc;
    desc.model                     = "island.x";
    desc.meshScale                  = 0.05f;
    desc.colliderFromMeshComponent = true;
    desc.colliderFlags             = toy::C_GROUND | toy::C_WALL | toy::C_CEILING;

    auto island = std::make_unique<toy::kit::StaticObject>(GetApp(), desc);
    island->SetPosition(pos);
    mStaticObjects.push_back(std::move(island));
}

void OutdoorScene::DeployHouse(const Vector3& pos)
{
    toy::kit::StaticObjectDesc desc;
    desc.model         = "house.x";
    desc.actorScale    = 0.003f;
    desc.colliderOffset = Vector3::Zero;
    desc.colliderScale  = Vector3(0.9f, 0.9f, 0.9f);
    desc.colliderFlags  = toy::C_WALL | toy::C_GROUND | toy::C_FOOT;
    desc.useGravity     = true;

    auto house = std::make_unique<toy::kit::StaticObject>(GetApp(), desc);
    house->SetPosition(pos);
    house->SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(150.0f)));
    mStaticObjects.push_back(std::move(house));
}

void OutdoorScene::DeployTree(const Vector3& pos)
{
    auto* actor = CreateActor<toy::Actor>();
    actor->SetPosition(pos);
    actor->SetScale(0.02f);

    auto* bb = actor->CreateComponent<toy::BillboardComponent>();
    bb->SetTexture(GetApp()->GetAssetManager()->GetTexture("tree.png"));
    bb->SetVisible(true);

    auto* coll = actor->CreateComponent<toy::ColliderComponent>();
    coll->GetBoundingVolume()->ComputeBoundingVolume(
        Vector3(-100.0f, -256.0f, -4.0f), Vector3(100.0f, 200.0f, 4.0f));
    coll->SetFlags(toy::C_WALL | toy::C_FOOT);

    actor->CreateComponent<toy::GravityComponent>();
}

void OutdoorScene::DeployMirror(const Vector3& pos)
{
    auto* actor = CreateActor<toy::Actor>();
    actor->SetPosition(pos);
    actor->SetScale(1.0f);
    actor->SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(-45.0f)));

    auto* capture = actor->CreateComponent<toy::SceneCaptureComponent>();
    capture->Init({.width = 512, .height = 512});
    capture->SetCaptureMode(toy::CaptureMode::Mirror);
    capture->SetSurfaceInfo({.scWidth = 10.0f, .scHeight = 10.0f});

    auto* surface = actor->CreateComponent<toy::RenderSurfaceComponent>();
    surface->SetTexture(capture->GetColorTexture());
    surface->SetScale(10.0f, 10.0f);
    surface->SetFlip(true, false);
    surface->SetSurfaceMode(toy::SurfaceMode::Mirror);
}
