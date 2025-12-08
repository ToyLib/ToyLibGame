#include "KitCore/SceneManager.h"
#include "KitCore/Game.h"

namespace toy::kit {

SceneManager::SceneManager(Game* game)
: mGame(game)
{
}

SceneManager::~SceneManager()
{
    if (mCurrent)
    {
        mCurrent->OnExit();
    }
}

void SceneManager::Change(std::unique_ptr<Scene> next)
{
    if (mCurrent)
    {
        mCurrent->OnExit();
    }

    mCurrent = std::move(next);

    if (mCurrent)
    {
        mCurrent->mGame = mGame;
        mCurrent->OnEnter();
    }
}

void SceneManager::Update(float deltaTime)
{
    if (mCurrent)
    {
        mCurrent->Update(deltaTime);
    }
}

} // namespace toy::kit
