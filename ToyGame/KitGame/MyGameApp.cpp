#include "MyGameApp.h"
#include "Engine/Core/ApplicationEntry.h"
#include "ToyLib.h"


MyGameApp::MyGameApp()
    : toy::Application()
{
    GetAssetManager()->SetAssetsPath("ToyGame/Assets/KitGame/");

}

void MyGameApp::InitGame()
{
    mGameFlow = std::make_unique<toy::kit::GameFlow>(this);
    mGameFlow->Init();
    mGameFlow->SetInitialScene(                                                   std::make_unique<TitleScene>());
}

void MyGameApp::ProcessInput(const toy::InputState& input)
{
    if (mGameFlow)
    {
        mGameFlow->ProcessInput(input);
    }
}

void MyGameApp::UpdateGame(float deltaTime)
{
    if (mGameFlow)
    {
        mGameFlow->Update(deltaTime);
    }
}
