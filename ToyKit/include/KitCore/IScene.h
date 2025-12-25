#pragma once

#include <vector>
#include <utility>

#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"

namespace toy::kit {

//============================================================
// SceneContext
//  - Scene が Application にアクセスするための最低限の情報
//============================================================
struct SceneContext
{
    toy::Application* app = nullptr;
};

//============================================================
// IScene
//  - ゲームの 1 状態（タイトル / ステージ / ムービー等）の基底
//  - Actor の生成と寿命管理を一元化する
//============================================================
class IScene
{
public:
    virtual ~IScene() = default;

    // シーン開始時に呼ばれる
    // 派生クラスは override する場合も、必ず親を呼ぶこと
    virtual void OnEnter(const SceneContext& ctx);

    // シーン終了時に呼ばれる
    // 生成した Actor はここで全て破棄される
    virtual void OnExit();

    // 入力処理（必要な Scene のみ override）
    virtual void ProcessInput(const InputState&) {}

    // 更新処理（必要な Scene のみ override）
    virtual void Update(float) {}

protected:
    //========================================================
    // Scene 管理下の Actor を生成する
    //  - Application::CreateActor() をラップ
    //  - 生成した Actor は自動的に Scene に登録される
    //========================================================
    template<class T, class... Args>
    T* CreateActor(Args&&... args)
    {
        T* actor = mApp->CreateActor<T>(std::forward<Args>(args)...);
        mActors.push_back(actor);
        return actor;
    }

protected:
    toy::Application* mApp = nullptr;

private:
    // Scene が生成した Actor 一覧
    std::vector<Actor*> mActors;

    // Scene 終了時の一括破棄
    void DestroyAllActors();
};

} // namespace toy::kit
