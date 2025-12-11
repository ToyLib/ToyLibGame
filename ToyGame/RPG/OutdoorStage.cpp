#include "OutdoorStage.h"

OutdoorStage::OutdoorStage(toy::Application* app)
: mApp(app)
{
    

}

OutdoorStage::~OutdoorStage()
{
    
}

void OutdoorStage::InitStage()
{
    DeployGround();
    DeploySky();
    
    // レンガ
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            DeployBrick(Vector3(-100 + 20*j/2 + 10*i*2, 20, -20 + 5*j*2), true);
        }
    }

    for (int i = 0; i < 5; i++)
    {
        DeployBrick(Vector3(0, 1+i*5, -50 + i*5), true);

    }


    DeployHouse(Vector3(-60, 0, 15));

    
    // 木（ビルボード）
    auto treeActor = mApp->CreateActor<toy::Actor>();
    treeActor->SetPosition(Vector3(20.0f, 4.5f, 0.0f));
    treeActor->SetScale(0.02);
    auto treeBillboard = treeActor->CreateComponent<toy::BillboardComponent>(100);
    treeBillboard->SetTexture(mApp->GetAssetManager()->GetTexture("tree.png"));
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
    auto fireActor = mApp->CreateActor<toy::Actor>();
    auto fireMesh = fireActor->CreateComponent<toy::MeshComponent>();
    fireMesh->SetMesh(mApp->GetAssetManager()->GetMesh("campfile.x"));
  
    fireActor->SetPosition(Vector3(-8, 0, -30));
    fireActor->SetScale(0.03f);
    auto fireCollider = fireActor->CreateComponent<toy::ColliderComponent>();
    fireCollider->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("campfile.x")->GetVertexArray());
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
    auto particleActor = mApp->CreateActor<toy::Actor>();
    particleActor->SetPosition(Vector3(0, 0, 0));
    auto particleComp = particleActor->CreateComponent<toy::ParticleComponent>();
    particleComp->SetTexture(mApp->GetAssetManager()->GetTexture("fire.png"));
    particleComp->CreateParticles(Vector3(0, 0, 0),
                                  10,
                                  1000,
                                  0.3f,
                                  5.5,
                                  toy::ParticleComponent::P_SMOKE);
    particleComp->SetAddBlend(true);
    particleActor->SetParent(fireActor);
    
    
    
    // 時間の設定
    mApp->GetTimeOfDaySystem()->SetTimeScale(0.0f);
    mApp->GetTimeOfDaySystem()->SetTime(8.0f);
}

void OutdoorStage::Update(float deltaTime)
{
    if (mWeather)
    {
        mWeather->Update(deltaTime);
    }
}

void OutdoorStage::DeployBrick(const Vector3& pos, bool bWall)
{
    auto actor = mApp->CreateActor<toy::Actor>();
    actor->SetPosition(pos);
    actor->SetScale(5.0f);
    
    auto mesh = actor->CreateComponent<toy::MeshComponent>();
    mesh->SetMesh(mApp->GetAssetManager()->GetMesh("brick.x"));
    

    auto coll = actor->CreateComponent<toy::ColliderComponent>();
    coll->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("brick.x")->GetVertexArray());
    
    if (bWall)
    {
        coll->SetFlags(toy::C_GROUND | toy::C_WALL);
    }
    else
    {
        coll->SetFlags(toy::C_GROUND);
    }
}

void OutdoorStage::DeployHouse(const Vector3& pos)
{
    // 建物
    auto towerActor = mApp->CreateActor<toy::Actor>();
    auto towerMesh = towerActor->CreateComponent<toy::MeshComponent>();
    towerMesh->SetMesh(mApp->GetAssetManager()->GetMesh("house.x"));
    
    auto towerCollider = towerActor->CreateComponent<toy::ColliderComponent>();
    towerCollider->GetBoundingVolume()->ComputeBoundingVolume(mApp->GetAssetManager()->GetMesh("house.x")->GetVertexArray());
    towerCollider->SetDisp(true);
    towerCollider->SetFlags(toy::C_WALL | toy::C_GROUND | toy::C_FOOT);
    towerCollider->GetBoundingVolume()->AdjustBoundingBox(Vector3(0,0,0), Vector3(0.9, 0.9, 0.9));
    towerActor->SetPosition(pos);
    towerActor->SetScale(0.003f);
    Quaternion(Vector3::UnitY, Math::ToRadians(150));
    towerActor->CreateComponent<toy::GravityComponent>();
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
