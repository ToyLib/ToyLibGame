#pragma once
#include "Engine/Core/Application.h"
#include "KitCore/GameFlow.h"

//=============================================================================
// GameRPG
//  RPG ゲームアプリ。KitGame の GameApp と同じ GameFlow 方式を採用。
//  シーン管理は GameFlow → OutdoorScene (IScene) に委譲。
//=============================================================================

class GameRPG : public toy::Application
{
public:
    GameRPG();
protected:
    void InitGame() override;
    void ProcessInput(const toy::InputState& input) override;
    void UpdateGame(float deltaTime) override;
    void ShutdownGame() override;
private:
    std::unique_ptr<toy::kit::GameFlow> mGameFlow;
};
