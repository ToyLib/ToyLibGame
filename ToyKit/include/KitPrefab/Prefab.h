#pragma once

#include "KitSignal/Signal.h"
#include "KitSignal/Events.h"
#include "Utils/MathUtil.h"

#include <string>

namespace toy {
class Application;
class Actor;
class ColliderComponent;
class GravityComponent;
class GroundConformSpriteComponent;
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
    void              SetPosition(const Vector3& pos);
    const Vector3&    GetPosition() const;

    void              SetRotation(const Quaternion& rot);
    const Quaternion& GetRotation() const;

    void  SetScale(float scale);
    float GetScale() const;

    const Matrix4&    GetWorldTransform() const;
    Vector3           GetForward() const;

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

    //---------------------------------------------------------------
    // 装飾ヘルパー（Creature/Humanoid など複数の Prefab から共通で使う）
    //  呼ばなければ何も生成されず、コストもゼロ。
    //---------------------------------------------------------------

    // 頭上に名前ビルボードを表示する（別 Actor を内部生成し、追従させる）
    void SetupNameBoard(const std::string& name, const std::string& fontPath,
                        float yOffset = 4.0f,
                        const Vector3& color = Vector3(1.0f, 0.0f, 0.0f));

    // ロックオン候補/ロック中の足元スプライトを表示する
    // ※ 呼ぶ前に TrackCollider() 済みであること。空文字なら該当スプライトは作らない。
    void SetupTargetSprites(const std::string& candidateTex,
                            const std::string& lockedTex = "");

    // 常時ループするアンビエントサウンドを再生する（唸り声・環境音など）
    void SetupAmbientSound(const std::string& soundPath,
                           float volume = 1.0f, bool loop = true);

    // 常時表示のテキストビルボードを本体に直接つける（吹き出し等）
    void SetupSpeechText(const std::string& text, const std::string& fontPath,
                         const Vector3& color = Vector3::One);

    // 派生Prefab固有の毎フレーム処理（必要なものだけ override）
    virtual void OnUpdate(float /*deltaTime*/) {}

private:
    friend class detail::PrefabActorAdapter;

    // 内部の Actor アダプタから毎フレーム呼ばれる
    void TickFromActor(float deltaTime);

    void DetectCollisionEvents();
    void DetectGroundedEvent();

    void UpdateNameBoard();
    void UpdateTargetSprites();

    toy::GroundConformSpriteComponent* CreateTargetSprite(const std::string& texPath);

    toy::Application* mApp    = nullptr;
    toy::Actor*        mActor = nullptr;

    toy::ColliderComponent* mCollider = nullptr;
    toy::GravityComponent*  mGravity  = nullptr;

    Signal<CollisionEvent> mOnCollision;
    Signal<GroundedEvent>  mOnGrounded;

    bool mWasGrounded = false;

    // 名前ビルボード（別 Actor）
    toy::Actor* mNameActor   = nullptr;
    float       mNameYOffset = 4.0f;

    // ターゲット表示スプライト
    toy::GroundConformSpriteComponent* mCandidateSigne = nullptr;
    toy::GroundConformSpriteComponent* mLockedSigne    = nullptr;
};

} // namespace toy::kit
