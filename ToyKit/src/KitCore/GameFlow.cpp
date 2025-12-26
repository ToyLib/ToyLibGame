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
        mCurrentScene->Unload();
    }
    mCurrentScene = std::move(next);

    if (mCurrentScene)
    {
        mCurrentScene->SetRequestChange([this](std::unique_ptr<IScene> s){
            this->ChangeScene(std::move(s));
        });

        SceneContext ctx{ mApp };
        mCurrentScene->Init(ctx);
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
    // まずは遷移を安全なタイミングで反映
    if (mPendingScene)
    {
        ApplyPendingScene();
    }

    if (mCurrentScene) mCurrentScene->Update(deltaTime);

    // Update中にRequestされたら、このフレーム末に適用するのもアリ
    if (mPendingScene)
    {
        ApplyPendingScene();
    }
}

void GameFlow::ApplyPendingScene()
{
    if (mCurrentScene) mCurrentScene->Unload();

    mCurrentScene = std::move(mPendingScene);

    if (mCurrentScene)
    {
        SceneContext ctx{ mApp };
        mCurrentScene->Init(ctx);
    }
}

} // namespace toy::kit
