#include "FieldScene.h"
#include "ToyLib.h"
#include "../Actors/PlayerActor.h"

FieldScene::FieldScene()
{
    
}

void FieldScene::InitScene()

{
    toy::PostEffectDesc effectDesc;
    effectDesc.type = toy::PostEffectType::None;
    effectDesc.intensity = 1.0f;
    //effectDesc.paperTex = mApp->GetAssetManager()->GetTexture("paper_tex.jpg");
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);
    
    // 時間の設定
    GetApp()->GetTimeOfDaySystem()->SetTimeScale(0.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(16.0f, 30.0f);
    
    
    // BGM
    GetApp()->GetSoundMixer()->LoadBGM("BGM/MusMus-BGM-112.mp3");
    GetApp()->GetSoundMixer()->PlayBGM();
    GetApp()->GetSoundMixer()->SetVolume(0.1);
    
    DeployGround();
    DeploySky();
    CreateActor<PlayerActor>();
}

void FieldScene::ProcessInput(const struct toy::InputState &input)
{
    
}

void FieldScene::Update(float deltaTime)
{
    if (mWeather)
    {
        mWeather->Update(deltaTime);
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
    meshComp->SetMesh(GetApp()->GetAssetManager()->GetMesh("Field/ground2.x"));
    actor->SetPosition(Vector3(0,0,0));
    actor->SetScale(1);
    meshComp->SetToonRender(false);
    meshComp->SetEnableShadow(false);
    
    auto groundMesh = GetApp()->GetAssetManager()->GetMesh("Field/ground2.x");
    auto va = groundMesh->GetVertexArray();
    auto vaList = groundMesh->GetVertexArray();
    for (auto& va : vaList)
    {
        actor->ComputeWorldTransform();
        const auto& polys = va->GetWorldPolygons(actor->GetWorldTransform());
        GetApp()->GetPhysWorld()->SetGroundPolygons(polys);
    }
}
