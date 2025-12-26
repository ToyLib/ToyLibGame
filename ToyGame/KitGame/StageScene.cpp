#include "StageScene.h"

void StageScene::InitScene()
{
    
    auto a = CreateActor<toy::Actor>();
    auto mesh = a->CreateComponent<toy::SkeletalMeshComponent>();
    mesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("Hero/hero_m.fbx"));
    a->SetScale(0.001f);
    mesh->GetAnimPlayer()->Play(17);
    
    
    toy::PostEffectDesc effectDesc;
    effectDesc.type = toy::PostEffectType::Sepia;
    effectDesc.intensity = 1.0f;
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);
}

void StageScene::Update(float delatTime)
{
    std::cout << "Scene Stage " << delatTime << std::endl;
}
