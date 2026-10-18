#include "GameApp.h"
#include "Engine/Core/ApplicationEntry.h"
#include "ToyLib.h"
#include "Scenes/FieldScene.h"
#include "Scenes/TitleScene.h"
#include "Scenes/StoryScene.h"
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

    // ポーズメニュー（Start で開閉。Scene ごとの項目は IScene::GetPauseMenuItems）
    toy::kit::PauseMenuDesc pause;
    pause.list.font = GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 22);
    pause.items = {
        { "タイトルへ", [this]() { mGameFlow->RequestChange(std::make_unique<TitleScene>()); } },
    };
    mGameFlow->EnablePauseMenu(pause);
    mGameFlow->SetInitialScene(std::make_unique<TitleScene>());
    //mGameFlow->SetInitialScene(std::make_unique<StageScene>());
    //mGameFlow->SetInitialScene(std::make_unique<FieldScene>());
    //mGameFlow->SetInitialScene(std::make_unique<SnowScene>());
}

void GameApp::ProcessInput(const toy::InputState& input)
{
    if (mGameFlow)
    {
        mGameFlow->ProcessInput(input);
    }
    
    if (input.Keyboard.GetKeyState(SDL_SCANCODE_F6) == toy::EPressed)
    {
        bool enableShadow = GetRenderer()->GetEnableShadow();
        GetRenderer()->SetEnableShadow(!enableShadow);
    }
    
}

void GameApp::UpdateGame(float deltaTime)
{
    if (mGameFlow)
    {
        mGameFlow->Update(deltaTime);
    }
}

void GameApp::ShutdownGame()
{
    if (mGameFlow)
    {
        mGameFlow->RequestChange(nullptr);
        mGameFlow.reset();
    }
    GetSoundMixer()->StopBGM();   
}
