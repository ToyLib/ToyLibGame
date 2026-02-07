#include "KitCore/GameFlow.h"
#include "KitCore/IScene.h"
#include "Engine/Runtime/InputSystem.h"
#include "Render/IRenderer.h"
#include "Engine/Core/Application.h"

namespace toy::kit {

GameFlow::GameFlow(toy::Application* app)
    : mApp(app)
    , mFadeAlpha(0.0f)
    , mFadeOutSec(1.25f)
    , mFadeInSec(1.25f)
{
    mCtx.app = app;
}

void GameFlow::Init()
{

}

void GameFlow::SetInitialScene(std::unique_ptr<IScene> scene)
{
    if (!scene)
    {
        return;
    }
    // 念のため既存シーンを破棄（基本は無い想定）
    if (mCurrentScene)
    {
        mCurrentScene->Unload();
    }

    mCurrentScene = std::move(scene);

    // フック仕込み
    AttachSceneHooks(mCurrentScene.get());

    // 初期化
    mCurrentScene->Init(mCtx);

    // 初回はフェードインだけやりたい場合
    mFadeAlpha = 1.0f;
    mApp->GetRenderer()->SetFade(mFadeAlpha);

    mState = FlowState::FadeIn;
}

void GameFlow::AttachSceneHooks(IScene* scene)
{
    if (!scene)
    {
        return;
    }
    
    scene->SetRequestChange(
        [this](std::unique_ptr<IScene> next)
        {
            this->RequestChange(std::move(next)); // ★フェード付きに統一
        }
    );
}

void GameFlow::RequestChange(std::unique_ptr<IScene> next)
{
    if (!next)
    {
        return;
    }
    // 連打時の方針：上書き（今はこれでOK）
    mPendingScene = std::move(next);

    // FadeOut 開始
    mFadeAlpha = 0.0f;
    mState = FlowState::FadeOut;

    // 念のため即反映
    mApp->GetRenderer()->SetFade(mFadeAlpha);
}
/*
void GameFlow::ChangeScene(std::unique_ptr<IScene> next)
{
    // 「即時切替」用途なら名前を ChangeSceneImmediate にしたい
    mPendingScene = std::move(next);
    ApplyPendingScene();

    // 即時切替後はフェード無しでRunningに戻す
    mFadeAlpha = 0.0f;
    mApp->GetRenderer()->SetFade(0.0f);
    mState = FlowState::Running;
}
*/
void GameFlow::ApplyPendingScene()
{
    if (!mPendingScene)
    {
        return;
    }
    if (mCurrentScene)
    {
        mCurrentScene->Unload();
    }

    mCurrentScene = std::move(mPendingScene);

    if (mCurrentScene)
    {
        AttachSceneHooks(mCurrentScene.get());  // ★忘れずに
        mCurrentScene->Init(mCtx);                    // ctxはメンバを使う
    }
}

void GameFlow::ProcessInput(const InputState& input)
{
    if (mState != FlowState::Running)
    {
        return;
    }
    if (mCurrentScene)
    {
        mCurrentScene->ProcessInput(input);
    }
}

void GameFlow::Update(float deltaTime)
{
    switch (mState)
    {
        case FlowState::Running:
            if (mCurrentScene)
            {
                mCurrentScene->Update(deltaTime);
            }
            break;

        case FlowState::FadeOut:
            mFadeAlpha += deltaTime / mFadeOutSec;
            if (mFadeAlpha >= 1.0f)
            {
                mFadeAlpha = 1.0f;
                mState = FlowState::SwitchScene;
            }
            mApp->GetRenderer()->SetFade(mFadeAlpha);
            break;

        case FlowState::SwitchScene:
            ApplyPendingScene();
            mState = FlowState::FadeIn;
            break;

        case FlowState::FadeIn:
            mFadeAlpha -= deltaTime / mFadeInSec;
            if (mFadeAlpha <= 0.0f)
            {
                mFadeAlpha = 0.0f;
                mState = FlowState::Running;
            }
            mApp->GetRenderer()->SetFade(mFadeAlpha);
            break;
    }
}

} // namespace toy::kit
