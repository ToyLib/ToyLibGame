#pragma once

#include <memory>
#include "KitCore/IScene.h"

namespace toy::kit {

class GameFlow
{
public:
    explicit GameFlow(toy::Application* app);

    // Scene を直接差し替える
    void ChangeScene(std::unique_ptr<IScene> next);

    void ProcessInput(const InputState& input);
    void Update(float deltaTime);

private:
    toy::Application* mApp = nullptr;
    std::unique_ptr<IScene> mCurrentScene;
};

} // namespace toy::kit
