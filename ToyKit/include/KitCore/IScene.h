#pragma once

#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"

#include <vector>
#include <utility>
#include <functional>

namespace toy::kit {

//============================================================
// SceneContext
//  - Scene が Application にアクセスするための最低限の情報
//============================================================
struct SceneContext
{
    toy::Application* app   = nullptr;
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
    void Init(const SceneContext& ctx);

    // シーン終了時に呼ばれる
    // 生成した Actor はここで全て破棄される
    void Unload();

    // 入力処理（必要な Scene のみ override）
    virtual void ProcessInput(const InputState&) {}

    // 更新処理（必要な Scene のみ override）
    virtual void Update(float deltaTime) {}
    
    
    using RequestChangeFn = std::function<void(std::unique_ptr<IScene>)>;

    void SetRequestChange(RequestChangeFn fn) { mRequestChange = std::move(fn); }

    

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

    void RequestChange(std::unique_ptr<IScene> next)
    {
        if (mRequestChange) mRequestChange(std::move(next));
    }
    
    // シーン構築の4フェーズ（この順で呼ばれる。IScene::Init() → InitScene() が呼び出す）。
    // 必要なものだけ override すればよい（既定は何もしない）。
    //   DefineEnvironment : ポストエフェクト/時刻/BGM等、見た目・雰囲気の設定
    //   DefineWorld       : 地形・StaticObject の配置など、動かない世界の構築
    //   SpawnCharacters   : Player/NPC（Agent<TPrefab>）のスポーン
    //   DefineUI          : UI Actor の構築
    // 「いつ・なぜスポーンするか」（時間経過・シナリオ進行等の条件）は各Scene固有の
    // C++コードとして書く（汎用的なスポーン条件フレームワークは今のところ用意しない）。
    virtual void DefineEnvironment() {}
    virtual void DefineWorld() {}
    virtual void SpawnCharacters() {}
    virtual void DefineUI() {}

    virtual void UnloadScene() {}

    
    // toy::Actor::GetApp() と同様に非 const で返す
    // （Prefab の生成など、非 const な Application API を呼ぶ場面があるため）
    toy::Application* GetApp() { return mApp; }

private:
    toy::Application* mApp = nullptr;
    // Scene が生成した Actor 一覧
    std::vector<Actor*> mActors;

    // シーン開始時に4フェーズを順番に呼ぶ（Init() から呼ばれる。派生Sceneは
    // このメソッド自体ではなく、上記の4フェーズを override する）
    void InitScene();

    // Scene 終了時の一括破棄
    void DestroyAllActors();
    
    RequestChangeFn mRequestChange;
};

} // namespace toy::kit
