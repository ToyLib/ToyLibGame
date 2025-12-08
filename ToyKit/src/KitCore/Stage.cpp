#include "KitCore/Stage.h"
#include "KitCore/Game.h"
#include "Engine/Core/Actor.h"

namespace toy::kit {

Stage::Stage(Game* game)
: mGame(game)
{
}

Stage::~Stage()
{
    // シーン切り替えなどで Stage が破棄されるとき、
    // このステージに属していた Actor もまとめて破棄依頼しておく
    DestroyAllActors();
}

bool Stage::LoadFromFile(const std::string& path)
{
    // TODO: JSON 等からステージ構成を読む処理をここに実装
    (void)path;
    return true;
}

void Stage::Unload()
{
    DestroyAllActors();
}

void Stage::RegisterActor(toy::Actor* actor)
{
    if (actor)
    {
        mActors.emplace_back(actor);
    }
}

void Stage::DestroyAllActors()
{
    if (!mGame)
    {
        mActors.clear();
        return;
    }

    for (auto* actor : mActors)
    {
        if (actor)
        {
            mGame->DestroyActor(actor);
        }
    }
    mActors.clear();
}

void Stage::Update(float deltaTime)
{
    (void)deltaTime;
    // ステージ固有のギミック更新などがあればここに追加
}

} // namespace toy::kit
