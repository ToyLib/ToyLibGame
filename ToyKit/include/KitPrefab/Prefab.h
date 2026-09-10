#pragma once

#include "KitSignal/Signal.h"
#include "KitSignal/Events.h"
#include "Utils/MathUtil.h"

namespace toy {
class Application;
class Actor;
class ColliderComponent;
class GravityComponent;
} // namespace toy

namespace toy::kit {

namespace detail { class PrefabActorAdapter; }

//=============================================================================
// Prefab
//  ToyKit の全 Prefab（Humanoid / Creature / StaticObject / Projectile）の基底。
//
//  設計方針:
//    - ゲーム側は toy::Actor を直接触らない（内部実装として隠蔽する）
//    - 実行中の変更は Interface（SetPosition 等、派生クラスが追加する）
//    - 発生した事実は Signal で通知する（OnCollision / OnGrounded）
//    - ゲーム上の意味（HP 等）は持たない
//=============================================================================
class Prefab
{
public:
    virtual ~Prefab();

    Prefab(const Prefab&)            = delete;
    Prefab& operator=(const Prefab&) = delete;

    //-------------------------------------------------------------------
    // Interface（共通分）
    //-------------------------------------------------------------------
    void           SetPosition(const Vector3& pos);
    const Vector3& GetPosition() const;

    virtual void SetVisible(bool visible)          = 0;
    virtual void SetCollisionEnabled(bool enabled) = 0;

    //-------------------------------------------------------------------
    // Signal
    //-------------------------------------------------------------------
    Signal<CollisionEvent>& OnCollision() { return mOnCollision; }
    Signal<GroundedEvent>&  OnGrounded()  { return mOnGrounded; }

protected:
    explicit Prefab(toy::Application* app);

    toy::Application* GetApp()   const { return mApp; }
    toy::Actor*        GetActor() const { return mActor; }

    // 派生Prefabが自分のコライダー/重力コンポーネントを登録する。
    // 登録すると毎フレーム Signal 検出の対象になる。
    void TrackCollider(toy::ColliderComponent* collider) { mCollider = collider; }
    void TrackGravity(toy::GravityComponent* gravity)    { mGravity  = gravity;  }

    // 派生Prefab固有の毎フレーム処理（必要なものだけ override）
    virtual void OnUpdate(float /*deltaTime*/) {}

private:
    friend class detail::PrefabActorAdapter;

    // 内部の Actor アダプタから毎フレーム呼ばれる
    void TickFromActor(float deltaTime);

    void DetectCollisionEvents();
    void DetectGroundedEvent();

    toy::Application* mApp    = nullptr;
    toy::Actor*        mActor = nullptr;

    toy::ColliderComponent* mCollider = nullptr;
    toy::GravityComponent*  mGravity  = nullptr;

    Signal<CollisionEvent> mOnCollision;
    Signal<GroundedEvent>  mOnGrounded;

    bool mWasGrounded = false;
};

} // namespace toy::kit
