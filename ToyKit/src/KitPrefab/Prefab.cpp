#include "KitPrefab/Prefab.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"

namespace toy::kit {

namespace detail {

//=============================================================================
// PrefabActorAdapter
//  Prefab を toy::Actor の更新ループに接続するための内部実装専用アダプタ。
//  ゲーム側・Prefab派生クラスの外からは見えない（Prefab.cpp 内に閉じる）。
//=============================================================================
class PrefabActorAdapter : public toy::Actor
{
public:
    PrefabActorAdapter(toy::Application* app, Prefab* owner)
        : toy::Actor(app)
        , mOwner(owner)
    {}

    void UpdateActor(float deltaTime) override
    {
        if (mOwner) mOwner->TickFromActor(deltaTime);
    }

    // Prefab が破棄されるときに呼ぶ。
    // Actor 自体は Dead マーク後も同フレーム中に Update() され得るため、
    // 参照先が既に破棄されたあとの UpdateActor() 呼び出しに備えて
    // 逆参照を確実に断つ。
    void ClearOwner() { mOwner = nullptr; }

private:
    Prefab* mOwner = nullptr;
};

} // namespace detail

Prefab::Prefab(toy::Application* app)
    : mApp(app)
{
    mActor = mApp->CreateActor<detail::PrefabActorAdapter>(this);
}

Prefab::~Prefab()
{
    if (mActor)
    {
        // Actor 側からの逆参照を先に断ってから Dead マークする
        static_cast<detail::PrefabActorAdapter*>(mActor)->ClearOwner();
        mActor->DestroyActor();
    }
}

void Prefab::SetPosition(const Vector3& pos)
{
    mActor->SetPosition(pos);
}

const Vector3& Prefab::GetPosition() const
{
    return mActor->GetPosition();
}

void Prefab::TickFromActor(float deltaTime)
{
    DetectCollisionEvents();
    DetectGroundedEvent();
    OnUpdate(deltaTime);
}

void Prefab::DetectCollisionEvents()
{
    if (!mCollider) return;

    for (auto* other : mCollider->GetTargetColliders())
    {
        mOnCollision.Emit(CollisionEvent{ other });
    }
}

void Prefab::DetectGroundedEvent()
{
    if (!mGravity) return;

    const bool grounded = mGravity->IsGrounded();
    if (grounded != mWasGrounded)
    {
        mWasGrounded = grounded;
        mOnGrounded.Emit(GroundedEvent{ grounded });
    }
}

} // namespace toy::kit
