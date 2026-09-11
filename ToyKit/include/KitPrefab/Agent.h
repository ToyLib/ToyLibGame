#pragma once

#include "KitPrefab/Prefab.h"
#include "KitPrefab/IBehavior.h"

#include <memory>
#include <utility>

namespace toy::kit {

//=============================================================================
// Agent<TPrefab>
//  TPrefab（Creature/Humanoid 等）1体分の Prefab と、その振る舞い
//  （IBehavior）を組み合わせて保持する汎用 Game Logic ラッパー。
//
//  TPrefab 固有の設定（ターゲット指定など）は、Agent に渡す前に
//  IBehavior 側で済ませておく（例: ChaseBehavior::SetTarget）。
//=============================================================================
template<class TPrefab>
class Agent
{
public:
    template<class... Args>
    Agent(toy::Application* app, std::unique_ptr<IBehavior> behavior, Args&&... args)
        : mBody(app, std::forward<Args>(args)...)
        , mBehavior(std::move(behavior))
    {
        mBody.OnCollision().Connect(
            [this](const CollisionEvent& e)
            {
                if (mBehavior) mBehavior->OnCollision(mBody, e);
            });

        if (mBehavior) mBehavior->OnStart(mBody);
    }

    void Update(float deltaTime)
    {
        if (mBehavior) mBehavior->OnUpdate(mBody, deltaTime);
    }

    void ProcessInput(const toy::InputState& state)
    {
        if (mBehavior) mBehavior->OnInput(mBody, state);
    }

    TPrefab&       GetBody()       { return mBody; }
    const TPrefab& GetBody() const { return mBody; }

    // 具体的な Behavior 型に触りたい場合（Signal を外から購読する等）用。
    // 普段は不要——Behavior 固有の設定は Agent に渡す前に済ませておく。
    IBehavior*       GetBehavior()       { return mBehavior.get(); }
    const IBehavior* GetBehavior() const { return mBehavior.get(); }

private:
    TPrefab                    mBody;
    std::unique_ptr<IBehavior> mBehavior;
};

} // namespace toy::kit
