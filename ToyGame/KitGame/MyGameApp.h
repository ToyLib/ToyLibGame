#include "Engine/Core/Application.h"
#include "KitCore/GameFlow.h"
#include "TitleScene.h"

class MyGameApp : public toy::Application
{
public:
    
    void InitGame() override
    {
        mGameFlow = std::make_unique<toy::kit::GameFlow>(this);

        // 最初の Scene をここで決定
        mGameFlow->ChangeScene(
            std::make_unique<TitleScene>()
        );
    }

    void ProcessInput(const toy::InputState& input) override
    {
        mGameFlow->ProcessInput(input);
    }

    void UpdateGame(float deltaTime) override
    {
        mGameFlow->Update(deltaTime);
    }

private:
    std::unique_ptr<toy::kit::GameFlow> mGameFlow;
};
