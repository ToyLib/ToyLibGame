#include "GameApp.h"
#include "Engine/Core/ApplicationEntry.h"
#include "ToyLib.h"
#include "Scenes/FieldScene.h"
#include "Scenes/TitleScene.h"
#include "Scenes/StageScene.h"
#include "Scenes/SnowScene.h"


GameApp::GameApp()
    : toy::Application()
{
    GetAssetManager()->SetAssetsPath("ToyGame/Assets/KitGame/");

}

void GameApp::InitGame()
{
    mGameFlow = std::make_unique<toy::kit::GameFlow>(this);
    mGameFlow->Init();
    //mGameFlow->SetInitialScene(std::make_unique<TitleScene>());
    //mGameFlow->SetInitialScene(std::make_unique<StageScene>());
    //mGameFlow->SetInitialScene(std::make_unique<FieldScene>());
    mGameFlow->SetInitialScene(std::make_unique<SnowScene>());
}

void GameApp::ProcessInput(const toy::InputState& input)
{
    if (mGameFlow)
    {
        mGameFlow->ProcessInput(input);
    }
}

void GameApp::UpdateGame(float deltaTime)
{
    if (mGameFlow)
    {
        mGameFlow->Update(deltaTime);
    }
}
