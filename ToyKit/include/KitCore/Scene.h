#pragma once

namespace toy::kit {

//class Game;
//class SceneManager;

/// =============================================
/// Scene
///  - 画面/状態の基本クラス
///  - TitleScene, FieldScene などをユーザーゲーム側で派生
/// =============================================
class Scene
{
public:
    virtual ~Scene() = default;

    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void Update(float deltaTime) {}

    class Game* GetGame() const { return mGame; }

private:
    friend class SceneManager;
    class Game* mGame = nullptr;  // Scene は Game の所有物ではない
};

} // namespace toy::kit
