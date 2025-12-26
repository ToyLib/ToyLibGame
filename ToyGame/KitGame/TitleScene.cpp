#include "TitleScene.h"
#include "ToyLib.h"
#include "StageScene.h"

void TitleScene::OnEnter(const toy::kit::SceneContext& ctx)
{
    IScene::OnEnter(ctx);
    std::cout << "[TitleScene] Enter\n";
    
    
    mLogoActor = CreateActor<toy::Actor>();
    mLogoMesh = mLogoActor->CreateComponent<toy::MeshComponent>();
    mLogoMesh->SetMesh(mApp->GetAssetManager()->GetMesh("Logo.x"));

}


void TitleScene::Update(float dt)
{
    std::cout << "[TitleScene] Update dt=" << dt << "\n";
}

void TitleScene::ProcessInput(const toy::InputState& input)
{
    if (input.Keyboard.GetKeyState(SDL_SCANCODE_RETURN) == toy::EPressed)
    {
        RequestChange(std::make_unique<StageScene>());
    }
}
