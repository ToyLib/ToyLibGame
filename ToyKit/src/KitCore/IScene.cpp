#include "KitCore/IScene.h"

namespace toy::kit {

//============================================================
// Init
//============================================================
void IScene::Init(const SceneContext& ctx)
{
    mApp = ctx.app;
    InitScene();
}

//============================================================
// InitScene
//  4フェーズをこの順で呼ぶ。派生Sceneは各フェーズだけを override する。
//============================================================
void IScene::InitScene()
{
    DefineEnvironment();
    DefineWorld();
    SpawnCharacters();
    DefineUI();
}

//============================================================
// Unload
//============================================================
void IScene::Unload()
{
    UnloadScene();
    
    DestroyAllActors();
    mApp = nullptr;
}

//============================================================
// DestroyAllActors
//============================================================
void IScene::DestroyAllActors()
{
    for (auto* actor : mActors)
    {
        if (actor)
        {
            actor->DestroyActor();
        }
    }
    mActors.clear();
}

} // namespace toy::kit
