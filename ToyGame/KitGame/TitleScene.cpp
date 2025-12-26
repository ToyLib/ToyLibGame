#include "TitleScene.h"
#include "ToyLib.h"
#include "StageScene.h"

TitleScene::TitleScene()
    : toy::kit::IScene()
    , mColor(0.0f)
{
    
}

void TitleScene::InitScene()
{
    std::cout << "[TitleScene] Enter\n";
    
    
    mLogoActor = CreateActor<toy::Actor>();
    mLogoMesh = mLogoActor->CreateComponent<toy::MeshComponent>();
    mLogoMesh->SetMesh(mApp->GetAssetManager()->GetMesh("Logo.x"));
    mLogoMesh->SetContourFactor(1.05f);

    
    toy::PostEffectDesc effectDesc;
    effectDesc.type = toy::PostEffectType::CRT;
    effectDesc.intensity = 1.0f;
    mApp->GetRenderer()->SetPostEffect(effectDesc);
}


void TitleScene::Update(float dt)
{
    mColor += dt;
    if (mColor > 1.0f)
    {
        mColor = 0.0f;
    }
    mLogoMesh->SetContourColor(Vector3(mColor, 0.0f, 0.0f));
}

void TitleScene::ProcessInput(const toy::InputState& input)
{
    if (input.Keyboard.GetKeyState(SDL_SCANCODE_RETURN) == toy::EPressed)
    {
        RequestChange(std::make_unique<StageScene>());
    }
}
