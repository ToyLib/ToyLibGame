#include "KitCore/GameFlow.h"
#include "Engine/Runtime/InputSystem.h"

namespace toy::kit {

GameFlow::GameFlow(toy::Application* app)
    : mApp(app)
{
}

void GameFlow::ChangeScene(std::unique_ptr<IScene> next)
{
    if (mCurrentScene)
    {
        mCurrentScene->OnExit();
    }

    mCurrentScene = std::move(next);

    if (mCurrentScene)
    {
        SceneContext ctx{ mApp };
        mCurrentScene->OnEnter(ctx);
    }
}

void GameFlow::ProcessInput(const toy::InputState& input)
{
    if (mCurrentScene)
    {
        mCurrentScene->ProcessInput(input);
    }
}

void GameFlow::Update(float deltaTime)
{
    if (mCurrentScene)
    {
        mCurrentScene->Update(deltaTime);
    }
}

} // namespace toy::kit
