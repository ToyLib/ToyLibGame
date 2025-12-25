#include "KitCore/IScene.h"

namespace toy::kit {

//============================================================
// OnEnter
//============================================================
void IScene::OnEnter(const SceneContext& ctx)
{
    mApp = ctx.app;
}

//============================================================
// OnExit
//============================================================
void IScene::OnExit()
{
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
            actor->SetState(toy::Actor::State::Dead);
        }
    }
    mActors.clear();
}

} // namespace toy::kit
