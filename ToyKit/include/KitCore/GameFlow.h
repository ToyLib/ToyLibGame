#pragma once

#include "KitCore/IScene.h"
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
    void ChangeScene(std::unique_ptr<class IScene> next);

    void ProcessInput(const struct InputState& input);
    void Update(float deltaTime);

private:
    void ApplyPendingScene();
    void AttachSceneHooks(class IScene* scene);

private:
    class toy::Application* mApp = nullptr;
    struct SceneContext mCtx;

    std::unique_ptr<class IScene> mCurrentScene;
    std::unique_ptr<class IScene> mPendingScene;

    FlowState mState = FlowState::Running;

    float mFadeAlpha;
    float mFadeOutSec;
    float mFadeInSec;
};

} // namespace toy::kit
