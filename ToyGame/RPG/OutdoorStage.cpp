#include "OutdoorStage.h"
#include "Actors/IslandActor.h"


OutdoorStage::OutdoorStage(toy::Application* app)
: mApp(app)
{
    

}

OutdoorStage::~OutdoorStage()
{
    
}

void OutdoorStage::InitStage()
{
    toy::PostEffectDesc effectDesc;
    effectDesc.type = toy::PostEffectType::Watercolor;
    effectDesc.intensity = 1.0f;
    //effectDesc.paperTex = mApp->GetAssetManager()->GetTexture("paper_tex.jpg");
    mApp->GetRenderer()->SetPostEffect(effectDesc);
    ;
    
    DeployGround();
    DeploySky();
    DeployFire(Vector3::Zero);
    
    // レンガ
    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            DeployIsland(Vector3(-100 + 20*j/2 + 10*i*2, 20, 20 + 5*j*2));
        }
    }

    for (int i = 0; i < 6; ++i)
    {
        DeployBrick(Vector3(0, -1+i*5, -15 + i*5));

    }


    DeployHouse(Vector3(-60, 0, 15));

    mApp->CreateActor<IslandActor>();
    
    // 木（ビルボード）
    auto treeActor = mApp->CreateActor<toy::Actor>();
    treeActor->SetPosition(Vector3(20.0f, 4.5f, 0.0f));
    treeActor->SetScale(0.02);
    auto treeBillboard = treeActor->CreateComponent<toy::BillboardComponent>(100);
    treeBillboard->SetTexture(mApp->GetAssetManager()->GetTexture("tree.png"));
    treeBillboard->SetVisible(true);
    treeActor->CreateComponent<toy::GravityComponent>();
    auto treeCollider = treeActor->CreateComponent<toy::ColliderComponent>();
    treeCollider->GetBoundingVolume()->ComputeBoundingVolume(Vector3(-100, -256, -4), Vector3(100,200,4));
    treeCollider->SetFlags(toy::C_WALL | toy::C_FOOT);
    // シャドウ用スプライト
    auto shadow = treeActor->CreateComponent<toy::ShadowSpriteComponent>(10);
    shadow->SetVisible(true);
    shadow->SetOffsetPosition(Vector3(0.0f, -4.5f, 0.0f));
    shadow->SetOffsetScale(0.03f);
    
    // 鏡を出す
    auto mirrorActor = mApp->CreateActor<toy::Actor>();
    mirrorActor->SetPosition(Vector3(-20.0f, 0.0f, 15.0f));
    mirrorActor->SetScale(1.0f);
    Quaternion q = Quaternion(Vector3::UnitY, Math::ToRadians(-45.0f));
    mirrorActor->SetRotation(q);
    auto capture = mirrorActor->CreateComponent<toy::SceneCaptureComponent>();
    capture->Init({.width=512, .height=512 });
    capture->SetCaptureMode(toy::CaptureMode::Mirror);

    auto mirrorComp = mirrorActor->CreateComponent<toy::RenderSurfaceComponent>();
    mirrorComp->SetTexture(capture->GetColorTexture());
    mirrorComp->SetScale(10.0f, 10.0f);
    capture->SetSurfaceInfo({ .scWidth=10.f, .scHeight=10.0f} );
    mirrorComp->SetFlip(true, true);
    mirrorComp->SetSurfaceMode(toy::SurfaceMode::Mirror);
    
 
    // 時間の設定
    mApp->GetTimeOfDaySystem()->SetTimeScale(0.0f);
    mApp->GetTimeOfDaySystem()->SetTime(12.0f, 0.0f);
}

void OutdoorStage::Update(float deltaTime)
{
    if (mWeather)
    {
        mWeather->Update(deltaTime);
    }
}

void OutdoorStage::DeployBrick(const Vector3& pos)
{
    auto actor = mApp->CreateActor<toy::Actor>();
    actor->SetPosition(pos);
    actor->SetScale(5.0f);
    
    auto mesh = actor->CreateComponent<toy::MeshComponent>();
    mesh->SetMesh(mApp->GetAssetManager()->GetMesh("brick.x"));
    

    auto coll = actor->CreateComponent<toy::ColliderComponent>();
    coll->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("brick.x")->GetVertexArray());
    
    coll->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_CEILING);

}

void OutdoorStage::DeployIsland(const Vector3& pos)
{
    auto actor = mApp->CreateActor<toy::Actor>();
    actor->SetPosition(pos);
    actor->SetScale(0.05f);
    
    auto mesh = actor->CreateComponent<toy::MeshComponent>();
    mesh->SetMesh(mApp->GetAssetManager()->GetMesh("island.x"));
    

    auto coll = actor->CreateComponent<toy::ColliderComponent>();
    coll->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("island.x")->GetVertexArray());
    
    coll->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_CEILING);

}

void OutdoorStage::DeployHouse(const Vector3& pos)
{
    // 建物
    auto towerActor = mApp->CreateActor<toy::Actor>();
    auto towerMesh = towerActor->CreateComponent<toy::MeshComponent>();
    towerMesh->SetMesh(mApp->GetAssetManager()->GetMesh("house.x"));
    
    auto towerCollider = towerActor->CreateComponent<toy::ColliderComponent>();
    towerCollider->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("house.x")->GetVertexArray());
    towerCollider->SetEnabled(true);
    towerCollider->SetFlags(toy::C_WALL | toy::C_GROUND | toy::C_FOOT);
    towerCollider->GetBoundingVolume()->AdjustBoundingBox(Vector3(0,0,0), Vector3(0.9, 0.9, 0.9));
    towerActor->SetPosition(pos);
    towerActor->SetScale(0.003f);
    Quaternion(Vector3::UnitY, Math::ToRadians(150));
    towerActor->CreateComponent<toy::GravityComponent>();
}

void OutdoorStage::DeployFire(const Vector3& pos)
{
    // 焚き火
    auto fireActor = mApp->CreateActor<toy::Actor>();
    auto fireMesh = fireActor->CreateComponent<toy::MeshComponent>();
    fireMesh->SetMesh(mApp->GetAssetManager()->GetMesh("campfile.x"));
  
    fireActor->SetPosition(Vector3(-8, 0, -30));
    fireActor->SetScale(0.03f);
    auto fireCollider = fireActor->CreateComponent<toy::ColliderComponent>();
    fireCollider->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("campfile.x")->GetVertexArray());
    fireCollider->SetEnabled(true);
    fireCollider->SetFlags(toy::C_GROUND | toy::C_WALL | toy::C_FOOT);
    fireActor->CreateComponent<toy::GravityComponent>();
    
    auto fireSound = fireActor->CreateComponent<toy::SoundComponent>();
    fireSound->SetSound("fire.wav");
    fireSound->SetLoop(true);
    fireSound->SetVolume(0.5f);
    fireSound->Enable3DSound(true);
    fireSound->Play();
    
    // ライト
    auto fireLight = fireActor->CreateComponent<toy::PointLightComponent>();
    fireLight->SetColor(Vector3(1.0f, 0.5f, 0.0f));

    
    // 炎
/*    auto particleActor = mApp->CreateActor<toy::Actor>();
    particleActor->SetPosition(Vector3(0, 0, 0));
    auto particleComp = particleActor->CreateComponent<toy::ParticleComponent>();
    particleComp->SetTexture(mApp->GetAssetManager()->GetTexture("fire.png"));
    particleComp->CreateParticles(Vector3(0, 0, 0),
                                  100,
                                  1000,
                                  0.6f,
                                  5.5,
                                  toy::ParticleComponent::P_SMOKE);
    particleComp->SetAddBlend(true);
    particleActor->SetParent(fireActor);
 */
    /*
    auto particleActorGPU = mApp->CreateActor<toy::Actor>();
    particleActorGPU->SetPosition(Vector3(0, 0, 0));
    auto particleCompGPU = particleActorGPU->CreateComponent<toy::GPUParticleComponent>();
    particleCompGPU->SetTexture(mApp->GetAssetManager()->GetTexture("fire.png"));
    particleCompGPU->CreateParticles(
                                     Vector3(0, 0, 0),
                                     10,
                                     1000,
                                     0.6f,
                                     5.5,
                                     toy::GPUParticleComponent::P_SMOKE);
    particleCompGPU->SetAddBlend(true);
    particleActorGPU->SetParent(fireActor);
*/
    /*
    using Particle = toy::GPUParticleComponent;

    // Actor
    auto particleActorGPU = mApp->CreateActor<toy::Actor>();
    particleActorGPU->SetPosition(Vector3(0, 10, 0));
    particleActorGPU->SetParent(fireActor);

    // Component
    auto* particle = particleActorGPU
        ->CreateComponent<Particle>();

    particle->SetTexture(
        mApp->GetAssetManager()->GetTexture("fire.png"));

    //----------------------------
    // パーティクル設定
    //----------------------------
    Particle::Desc desc;
    desc.maxParticles     = 10;          // num
    desc.particleLife     = 0.6f;         // partLife
    desc.size             = 5.5f;         // size
    desc.mode             = Particle::ParticleMode::Spark;
    desc.componentLife    = 1000.0f;      // life (0 = infinite)

    // 無限に出続ける挙動（旧仕様に近い）
    desc.spawnRatePerSec  = 6.0f;         // 発生頻度
    desc.spawnRampSec     = 0.6f;         // 立ち上がり
    desc.additiveBlend    = true;         // SetAddBlend(true)
    desc.warmStart        = true;         // 初期塊防止

    //----------------------------
    // 初期化 & 開始
    //----------------------------
    particle->Init(desc);
    particle->SetEmitterOffset(Vector3(0, 0, 0)); // 旧 CreateParticles の pos
    particle->Start();
    */
    
    // Actor
    auto particleActorGPU = mApp->CreateActor<toy::Actor>();
    particleActorGPU->SetPosition(Vector3(0, 0, 0));
    particleActorGPU->SetParent(fireActor);

    // Component
    auto* particle = particleActorGPU
        ->CreateComponent<toy::GPUParticleComponent>();

    particle->SetTexture(
        mApp->GetAssetManager()->GetTexture("fire.png"));
    
    //==============================
    // Desc で全設定
    //==============================
    toy::GPUParticleComponent::Desc desc;

    // --- 基本 ---
    desc.maxParticles   = 100;        // 旧 num
    desc.particleLife   = 0.6f;       // 旧 partLife
    desc.size           = 5.5f;       // 旧 size
    desc.mode           = toy::GPUParticleComponent::ParticleMode::Smoke;

    // --- エミッタ ---
    desc.emitterOffset  = Vector3(0.0f, 0.0f, 0.0f); // Actor ローカル
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

    
    //particle->Init(desc);
    particle->InitFromFile("ToyGame/Settings/Fire.json");
    particle->Start();
    
    
}

void OutdoorStage::DeployGround()
{
    // 地面
    auto b = mApp->CreateActor<toy::Actor>();
    auto g = b->CreateComponent<toy::MeshComponent>(false);
    g->SetMesh(mApp->GetAssetManager()->GetMesh("ground2.x"));
    b->SetPosition(Vector3(0,0,0));
    b->SetScale(1);
    g->SetToonRender(false);
    g->SetEnableShadow(false);
    
    auto groundMesh = mApp->GetAssetManager()->GetMesh("ground2.x");
    auto va = groundMesh->GetVertexArray();
    auto vaList = groundMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        b->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(b->GetWorldTransform());
        mApp->GetPhysWorld()->SetGroundPolygons(polys);
    }
}

void OutdoorStage::DeploySky()
{
    // スカイドーム
    auto skyActor = mApp->CreateActor<toy::Actor>();
    auto dome = skyActor->CreateComponent<toy::WeatherDomeComponent>();
    // オーバーレイ
    auto overlay = skyActor->CreateComponent<toy::WeatherOverlayComponent>();
    
    mWeather = std::make_unique<toy::kit::WeatherManager>();
    mWeather->SetWeatherDome(dome);
    mWeather->SetWeatherOverlay(overlay);
    mWeather->ChangeWeather(toy::WeatherType::CLEAR);
}
