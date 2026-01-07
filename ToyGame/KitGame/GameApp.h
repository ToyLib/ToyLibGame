#include "Engine/Core/Application.h"
#include "KitCore/GameFlow.h"
#include "TitleScene.h"

class GameApp : public toy::Application
{
public:
    GameApp();
    void InitGame() override;
    void ProcessInput(const toy::InputState& input) override;
    void UpdateGame(float deltaTime) override;
    
private:
    std::unique_ptr<toy::kit::GameFlow> mGameFlow;
};

