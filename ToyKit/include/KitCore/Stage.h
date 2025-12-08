#pragma once

#include "KitCore/Game.h"
#include "Engine/Core/Actor.h"

#include <vector>
#include <string>

/*
namespace toy {
    class Actor;
}
 */

namespace toy::kit {

class Game;

/// =============================================
/// Stage
///  - 特定の「ステージ／マップ」に属する Actor をまとめて管理するラッパ
///  - Actor の生成／破棄は必ず Game(Application) 経由で行い、
///    Stage は「このステージに属している Actor のリスト」だけを持つ
/// =============================================
class Stage
{
public:
    explicit Stage(Game* game = nullptr);
    ~Stage();

    // 後から Game を差し替えたい場合に使用
    void SetGame(Game* game) { mGame = game; }

    // ステージ設定を読み込む（JSON 等）
    bool LoadFromFile(const std::string& path);

    // このステージが管理している Actor をすべて破棄依頼する
    void Unload();

    // Game 経由で Actor を生成し、このステージに登録するヘルパー
    template <typename T, typename... Args>
    T* CreateActor(Args&&... args)
    {
        if (!mGame)
        {
            return nullptr;
        }

        T* actor = mGame->CreateActor<T>(std::forward<Args>(args)...);
        if (actor)
        {
            mActors.emplace_back(actor);
        }
        return actor;
    }

    // すでに外部で CreateActor 済みの Actor を「このステージの一員」として登録したい場合
    void RegisterActor(toy::Actor* actor);

    // ステージ固有のギミック更新などを入れたい場合用（今はダミー）
    void Update(float deltaTime);

private:
    // このステージが関知している Actor をすべて DestroyActor する
    void DestroyAllActors();

    Game* mGame = nullptr;                 // 所有しない
    std::vector<toy::Actor*> mActors;      // このステージに属する Actor 一覧
};

} // namespace toy::kit
