#pragma once
#include "Engine/Core/Application.h"


namespace toy::kit{



class Game : public toy::Application
{
public:
    Game();
    virtual ~Game() = default;

protected:
    /// ゲーム開始時（InitGame 内で呼ばれる）
    virtual void Setup() {}

    /// 毎フレーム Game 全体（Scene 更新より前）に呼ばれる
    virtual void Tick(float deltaTime) {}

    /// SceneManager アクセス
    //SceneManager&       scenes()       { return *mSceneManager; }
    //const SceneManager& scenes() const { return *mSceneManager; }

private:
    /// Application ライフサイクル制御
    void InitGame() override final;
    void UpdateGame(float deltaTime) override final;
    void ShutdownGame() override final;

    //std::unique_ptr<SceneManager> mSceneManager;
};

} // namespace toy::kit
