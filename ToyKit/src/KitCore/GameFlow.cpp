#include "KitCore/GameFlow.h"
#include "KitCore/IScene.h"
#include "Engine/Runtime/InputSystem.h"
#include "Render/IRenderer.h"
#include "Engine/Core/Application.h"
#include "UI/UIStack.h"

namespace toy::kit {

namespace {
void WaitRendererIdleSafe(toy::Application* app)
{
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    renderer->WaitIdle();
}
} // namespace

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
        WaitRendererIdleSafe(mApp);
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
    // ポーズメニューから遷移した場合など、開いたままにしない
    ClosePauseMenu();

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
        WaitRendererIdleSafe(mApp);
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
    // ポーズメニューの開閉（Scene には流さない）
    if (HandlePauseToggle(input))
    {
        return;
    }
    // Modal な UI（メッセージ / メニュー等）が入力を占有している間はシーンに流さない
    if (auto* ui = mApp->GetUIStack(); ui && ui->IsInputCaptured())
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
            // PausesGame な UI（確認ダイアログ等）が開いている間はシーンを止める
            if (auto* ui = mApp->GetUIStack(); ui && ui->IsGamePaused())
            {
                break;
            }
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

//============================================================
// ポーズメニュー
//============================================================
void GameFlow::EnablePauseMenu(const PauseMenuDesc& desc)
{
    mPauseDesc    = desc;
    mPauseEnabled = true;

    auto listDesc = mPauseDesc.list;
    listDesc.pauseGame   = true;   // ポーズメニューなので常に止める
    listDesc.allowCancel = true;   // B で再開

    // Scene の CreateActor ではなく Application に直接作る（Scene 切り替えで破棄されない）
    if (!mPauseMenu)
    {
        mPauseMenu = mApp->CreateActor<toy::ChoiceListActor>(listDesc);
    }
}

bool GameFlow::IsPauseMenuOpen() const
{
    return mPauseMenu && mPauseMenu->IsOpen();
}

void GameFlow::ClosePauseMenu()
{
    if (IsPauseMenuOpen())
    {
        mPauseMenu->Close();
    }
}

bool GameFlow::HandlePauseToggle(const InputState& input)
{
    if (!mPauseEnabled || !mPauseMenu)
    {
        return false;
    }
    if (!input.IsButtonPressed(mPauseDesc.toggleButton))
    {
        return false;
    }

    auto* ui = mApp->GetUIStack();

    // 開いている（かつ一番上にいる）ならもう一度押して再開
    // ・確認ダイアログ等が上に重なっている間は閉じない
    if (IsPauseMenuOpen())
    {
        if (ui && ui->GetTop() == mPauseMenu)
        {
            ClosePauseMenu();
            return true;
        }
        return false;
    }

    // 他の UI が入力を占有している間（会話中など）や、ポーズ禁止の Scene では開かない
    if (ui && ui->IsInputCaptured())
    {
        return false;
    }
    if (!mCurrentScene || !mCurrentScene->AllowsPause())
    {
        return false;
    }

    OpenPauseMenu();
    return true;
}

void GameFlow::OpenPauseMenu(int initialIndex)
{
    // 再開 → Scene の項目 → ゲーム全体の項目
    std::vector<PauseMenuItem> items;
    items.push_back({ mPauseDesc.resumeText, nullptr, true });

    if (mCurrentScene)
    {
        for (auto& item : mCurrentScene->GetPauseMenuItems())
        {
            items.push_back(std::move(item));
        }
    }
    for (const auto& item : mPauseDesc.items)
    {
        items.push_back(item);
    }

    std::vector<std::string> labels;
    labels.reserve(items.size());
    for (const auto& item : items)
    {
        labels.push_back(item.label);
    }

    mPauseMenu->Open(labels,
                     [this, items](int index)
                     {
                         const PauseMenuItem& item = items[index];

                         // メニューは決定時に閉じている。開いたままにする項目は開き直す
                         // （閉じる途中から開き直すので、見た目は開いたまま）
                         if (!item.closeMenu)
                         {
                             OpenPauseMenu(index);
                         }
                         if (item.onSelect)
                         {
                             item.onSelect();
                         }
                     },
                     nullptr,   // B：そのまま再開
                     initialIndex);
}

} // namespace toy::kit
