#pragma once

#include "Engine/Core/Component.h"
#include "Physics/ColliderFlags.h"
#include "Asset/Geometry/Polygon.h"
#include "Physics/PhysWorld.h"

#include <vector>
#include <memory>

namespace toy {



//------------------------------------------------------------------------------
// ColliderComponent
//------------------------------------------------------------------------------
// ・Actor にアタッチされる「当たり判定」用コンポーネント。
// ・生成時に自動で BoundingVolumeComponent を追加し、PhysWorld へ登録する。
// ・mFlags によって「自分が何者か」をビットフラグで保持する。
// ・PhysWorld 側から Collided() が呼ばれ、フレーム中に衝突した相手リストを保持する。
//------------------------------------------------------------------------------
class ColliderComponent : public Component
{
public:
    ColliderComponent(class Actor* a);
    virtual ~ColliderComponent();
    
    //--------------------------------------------------------------------------
    // 自分のコライダーフラグの操作
    //--------------------------------------------------------------------------
    // フラグ操作
    //--------------------------------------------------------------------------
    void SetFlags(uint32_t flags)          { mFlags = flags; }
    void AddFlag(uint32_t flag)            { mFlags |= flag; }
    void RemoveFlag(uint32_t flag)         { mFlags &= ~flag; }

    // 単一フラグ判定（C_PLAYER_TEAM など）
    bool HasFlag(uint32_t flag) const
    {
        return (mFlags & flag) != 0;
    }

    // 複数のうち「どれか1つ」
    bool HasAnyFlag(uint32_t mask) const
    {
        return (mFlags & mask) != 0;
    }

    // 複数すべて含む（TEAM | HURTBOX 等）
    bool HasAllFlags(uint32_t mask) const
    {
        return (mFlags & mask) == mask;
    }

    uint32_t GetFlags() const { return mFlags; }
    
    //--------------------------------------------------------------------------
    // 衝突情報
    //--------------------------------------------------------------------------
    // PhysWorld から「このコライダーと当たったよ」と通知される。
    // ・同一相手は重複登録しない。
    // ・mIsCollided フラグも立てる。
    void Collided(ColliderComponent* c);
    
    // 当フレーム中に衝突した相手一覧（PhysWorld が埋める）
    const std::vector<ColliderComponent*>& GetTargetColliders() const { return mTargetColliders; }
    
    // 衝突バッファをクリア（毎フレーム PhysWorld 側から呼ぶ想定）
    void ClearCollidBuffer() { mTargetColliders.clear(); }
    
    void Update(float deltaTime) override;
    
    // 自前の BoundingVolume を取得
    class BoundingVolumeComponent* GetBoundingVolume() const { return mBoundingVolume; }
    
    // 現在衝突状態かどうか（少なくとも1つ以上当たっているか）
    bool GetCollided() const { return mIsCollided; }
    void SetCollided(bool b) { mIsCollided = b; }
    
    // 有効/無効
    bool GetEnabled() const { return mEnabled; }
    void SetEnabled(bool b) { mEnabled = b; }
    
    // レイを取得（レイコライダー用に派生クラスで override する）
    virtual Ray GetRay() const { return Ray(); }
    
    void SetIsTrigger(bool b) { mIsTrigger = b; }
    bool IsTrigger() const    { return mIsTrigger; }
    
private:
    // 少なくとも 1 つ以上のコライダーと当たっているか
    bool mIsCollided;
    
    // 判定を行うかどうかのフラグ
    bool mEnabled;
    
    // 自動で生成されたバウンディングボリューム
    class BoundingVolumeComponent* mBoundingVolume;
    
    // 自分のコライダー種別（ビットフラグ）
    uint32_t mFlags;
    
    // このフレーム中に衝突した相手の一覧
    std::vector<ColliderComponent*> mTargetColliders;
    
    // Trigger当たっても反応しない
    bool mIsTrigger;
};

} // namespace toy
