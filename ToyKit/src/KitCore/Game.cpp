#include "KitCore/Game.h"
//#include "Kit/SceneManager.h"
//#include "Kit/Scene.h"

namespace toy::kit {

Game::Game()
: toy::Application()
{
    //mSceneManager = std::make_unique<SceneManager>(this);
}

void Game::InitGame()
{
    Setup();
}

void Game::UpdateGame(float deltaTime)
{
    Tick(deltaTime);
    //mSceneManager->Update(deltaTime);
}

void Game::ShutdownGame()
{
    //mSceneManager.reset();
}

} // namespace toy::kit
