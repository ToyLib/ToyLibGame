#pragma once

#include "KitCore/IScene.h"
#include "KitCore/PauseMenu.h"
#include <memory>

namespace toy::kit {

enum class FlowState
{
    Running,
    FadeOut,
    SwitchScene,
    FadeIn
};


class GameFlow
{
public:
    explicit GameFlow(class toy::Application* app);

    void Init(); // ← 今は空でOK

    void SetInitialScene(std::unique_ptr<class IScene> scene);

    void RequestChange(std::unique_ptr<class IScene> next);
    //void ChangeScene(std::unique_ptr<class IScene> next);

    void ProcessInput(const struct InputState& input);
    void Update(float deltaTime);

    //--------------------------------------------------------
    // ポーズメニュー（標準機能。使うゲームだけ有効にする）
    //  - toggleButton で開閉。どの Scene でも同じメニューを使い回す
    //  - 項目：再開 → Scene::GetPauseMenuItems() → desc.items
    //  - Scene::AllowsPause() が false の Scene では開かない
    //--------------------------------------------------------
    void EnablePauseMenu(const PauseMenuDesc& desc);
    bool IsPauseMenuOpen() const;
    void ClosePauseMenu();

private:
    void ApplyPendingScene();
    void AttachSceneHooks(class IScene* scene);

    void OpenPauseMenu(int initialIndex = 0);
    bool HandlePauseToggle(const struct InputState& input);

private:
    class toy::Application* mApp = nullptr;
    struct SceneContext mCtx;

    std::unique_ptr<class IScene> mCurrentScene;
    std::unique_ptr<class IScene> mPendingScene;

    FlowState mState = FlowState::Running;

    // ポーズメニュー（Actor は Application が所有。Scene をまたいで使い回す）
    bool                     mPauseEnabled { false };
    PauseMenuDesc            mPauseDesc;
    toy::ChoiceListActor*    mPauseMenu { nullptr };

    float mFadeAlpha;
    float mFadeOutSec;
    float mFadeInSec;
};

} // namespace toy::kit
