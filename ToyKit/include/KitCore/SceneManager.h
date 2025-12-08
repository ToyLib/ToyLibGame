#pragma once

#include <memory>
#include "KitCore/Scene.h"
#include "KitCore/Game.h"

namespace toy::kit {

/// =============================================
/// SceneManager
///  - 現在のSceneを保持し、切り替えと毎フレーム更新を管理
/// =============================================
class SceneManager
{
public:
    SceneManager(Game* game);
    ~SceneManager();

    void Change(std::unique_ptr<Scene> next);
    void Update(float deltaTime);

    Scene*       GetCurrent()       { return mCurrent.get(); }
    const Scene* GetCurrent() const { return mCurrent.get(); }

private:
    Game* mGame = nullptr;
    std::unique_ptr<class Scene> mCurrent;
};

} // namespace toy::kit
