#include "OutdoorScene.h"
#include "ToyLib.h"
#include "../Actors/Hero.h"
#include "../Actors/Wolf.h"
#include "../Actors/Shiro.h"
#include "../Actors/Stan.h"
#include "../Actors/MagicBolt.h"
#include "../Actors/HealBurst.h"
#include "../Actors/IslandActor.h"

#include <vector>

OutdoorScene::OutdoorScene()  = default;
OutdoorScene::~OutdoorScene() = default;

//-----------------------------------------------------------------------------
// DefineEnvironment — ポストエフェクト / 時間帯 / BGM
//-----------------------------------------------------------------------------
void OutdoorScene::DefineEnvironment()
{
    toy::PostEffectDesc effectDesc;
    effectDesc.type      = toy::PostEffectType::FeilyLand;
    effectDesc.intensity = 1.0f;
    effectDesc.paperTex  = GetApp()->GetAssetManager()->GetTexture("paper_tex.jpg");
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);

    GetApp()->GetTimeOfDaySystem()->SetTimeScale(10000.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(12.0f, 30.0f);

    GetApp()->GetSoundMixer()->LoadBGM("MusMus-BGM-112.ogg");
    GetApp()->GetSoundMixer()->PlayBGM();
    GetApp()->GetSoundMixer()->SetBgmVolume(0.5f);
    GetApp()->GetSoundMixer()->SetMasterVolume(0.8f);
}

//-----------------------------------------------------------------------------
// DefineWorld — 地形/プロップ（InitField）
//-----------------------------------------------------------------------------
void OutdoorScene::DefineWorld()
{
    InitField();
}

//-----------------------------------------------------------------------------
// SpawnCharacters — Hero/Wolf/Shiro/Stan
//-----------------------------------------------------------------------------
void OutdoorScene::SpawnCharacters()
{
    // プレイヤー（新方針: Humanoid Prefab を内包する Game Logic）
    mHero = MakeHero(GetApp());

    // Hero が発動した魔法/回復を Scene 側でエフェクト化する
    // （Hero 自身はエフェクトの生成・寿命管理を持たない）
    mHero->GetControlBehavior().OnCastMagic().Connect(
        [this](const CastMagicEvent& e)
        {
            mMagicBolts.push_back(std::make_unique<MagicBolt>(GetApp(), e.position, e.forward));
        });
    mHero->GetControlBehavior().OnCastHeal().Connect(
        [this](const CastHealEvent& e)
        {
            mHealBursts.push_back(std::make_unique<HealBurst>(GetApp(), e.position));
        });

    // Wolf x5（プレイヤーをターゲットに。Humanoid + ChaseBehavior の Agent）
    for (int i = 0; i < 5; ++i)
    {
        auto wolf = MakeWolf(GetApp(), &mHero->GetBody());
        wolf->GetBody().SetPosition(Vector3(-20.0f + i * 10.0f, 3.0f, -20.0f));
        mWolves.push_back(std::move(wolf));
    }

    // Shiro（焚き火の向かい側。Humanoid + ChaseBehavior の Agent）
    mShiro = MakeShiro(GetApp(), &mHero->GetBody());
    mShiro->GetBody().SetPosition(Vector3(0.0f, 0.0f, -25.0f));

    // Stan（プレイヤーに付いて歩くお供NPC。Humanoid + FollowBehavior の Agent。
    // Behavior のテストベットも兼ねる）
    mStan = MakeStan(GetApp(), &mHero->GetBody());
}

//-----------------------------------------------------------------------------
// DefineUI — ヘルスバー / 時刻表示
//-----------------------------------------------------------------------------
void OutdoorScene::DefineUI()
{
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
    }

    if (mShiro)
    {
        mShiro->Update(deltaTime);
    }

    if (mStan)
    {
        mStan->Update(deltaTime);
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
        (void)m;
        mTextComp->SetFormat("時刻 {:02} : {:02}  \n", h, 0);
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
    DeployBricks();

    // 島（レンガ状配置）
    DeployIslands();

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

//-----------------------------------------------------------------------------
// DeployBricks / DeployIslands — Desc化されたデータ（StaticObjectPlacement の
// 配列）として配置を組み立て、まとめて生成する（設計方針16: Scene構築のDesc化）。
//-----------------------------------------------------------------------------
void OutdoorScene::DeployBricks()
{
    toy::kit::StaticObjectDesc brickDesc;
    brickDesc.model         = "brick.x";
    brickDesc.actorScale    = 5.0f;
    brickDesc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_CEILING;

    std::vector<toy::kit::StaticObjectPlacement> placements;
    for (int i = 0; i < 6; ++i)
    {
        placements.push_back({ brickDesc, Vector3(0.0f, -1.0f + i * 5.0f, -15.0f + i * 5.0f) });
    }

    for (auto& obj : toy::kit::MakeStaticObjects(GetApp(), placements))
    {
        mStaticObjects.push_back(std::move(obj));
    }
}

void OutdoorScene::DeployIslands()
{
    toy::kit::StaticObjectDesc islandDesc;
    islandDesc.model                     = "island.x";
    islandDesc.meshScale                  = 0.05f;
    islandDesc.colliderFromMeshComponent = true;
    islandDesc.colliderFlags             = toy::C_GROUND | toy::C_WALL | toy::C_CEILING;

    std::vector<toy::kit::StaticObjectPlacement> placements;
    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            placements.push_back({ islandDesc, Vector3(-100.0f + 20.0f * j / 2.0f + 10.0f * i * 2.0f,
                                                        20.0f,
                                                        20.0f + 5.0f * j * 2.0f) });
        }
    }

    for (auto& obj : toy::kit::MakeStaticObjects(GetApp(), placements))
    {
        mStaticObjects.push_back(std::move(obj));
    }
}

void OutdoorScene::DeployHouse(const Vector3& pos)
{
    toy::kit::StaticObjectPlacement placement;
    placement.desc.model          = "house.x";
    placement.desc.actorScale     = 0.003f;
    placement.desc.colliderOffset = Vector3::Zero;
    placement.desc.colliderScale  = Vector3(0.9f, 0.9f, 0.9f);
    placement.desc.colliderFlags  = toy::C_WALL | toy::C_GROUND | toy::C_FOOT;
    placement.desc.useGravity     = true;
    placement.position = pos;
    placement.rotation = Quaternion(Vector3::UnitY, Math::ToRadians(150.0f));

    mStaticObjects.push_back(toy::kit::MakeStaticObject(GetApp(), placement));
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
