#include "GameRPG.h"
#include "Engine/Core/ApplicationEntry.h"
#include "ToyLib.h"
#include "Scenes/OutdoorScene.h"

GameRPG::GameRPG()
    : toy::Application()
{
    GetAssetManager()->SetAssetsPath("ToyGame/Assets/RPG/");
}

void GameRPG::InitGame()
{
    mGameFlow = std::make_unique<toy::kit::GameFlow>(this);
    mGameFlow->Init();
    mGameFlow->SetInitialScene(std::make_unique<OutdoorScene>());
}

void GameRPG::ProcessInput(const toy::InputState& input)
{
    if (mGameFlow)
    {
        mGameFlow->ProcessInput(input);
    }
}

void GameRPG::UpdateGame(float deltaTime)
{
    if (mGameFlow)
    {
        mGameFlow->Update(deltaTime);
    }
}

void GameRPG::ShutdownGame()
{
    if (mGameFlow)
    {
        mGameFlow->RequestChange(nullptr);
        mGameFlow.reset();
    }
}
