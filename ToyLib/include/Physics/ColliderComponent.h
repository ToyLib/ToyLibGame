#pragma once

#include "Engine/Core/Component.h"
#include "Asset/Geometry/Polygon.h"

#include <vector>
#include <memory>

namespace toy {

//------------------------------------------------------------------------------
// ビットマスク enum 用ヘルパーマクロ
//------------------------------------------------------------------------------
// enum class ではなく通常の enum を bitflag として扱うための演算子群。
// ColliderType のように「複数種類の属性を OR で持つ」用途で使用する。
// 例） C_PLAYER | C_FOOT など
//------------------------------------------------------------------------------
#define ENABLE_BITMASK_OPERATORS(x)                                \
inline x operator|(x a, x b) { return static_cast<x>(static_cast<int>(a) | static_cast<int>(b)); } \
inline x operator&(x a, x b) { return static_cast<x>(static_cast<int>(a) & static_cast<int>(b)); } \
inline x& operator|=(x& a, x b) { a = a | b; return a; }            \
inline x& operator&=(x& a, x b) { a = a & b; return a; }            \
inline x operator~(x a) { return static_cast<x>(~static_cast<int>(a)); }

//------------------------------------------------------------------------------
// ColliderType
//------------------------------------------------------------------------------
// 衝突カテゴリをビットフラグで表現する。
// 1つの Collider が複数フラグを同時に持つことも可能。
// 例）プレイヤーの足用コライダー: C_PLAYER | C_FOOT
//------------------------------------------------------------------------------
enum ColliderType : uint32_t
{
    C_NONE    = 0,
    C_PLAYER  = 1 << 0,
    C_ENEMY   = 1 << 1,
    C_BULLET  = 1 << 2,
    C_LASER   = 1 << 3,
    C_WALL    = 1 << 4,
    C_GROUND  = 1 << 5,
    C_FOOT    = 1 << 6,
};
ENABLE_BITMASK_OPERATORS(ColliderType)

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
    // ※現状は uint32_t で扱っているが、ColliderType を OR した値を想定。
    void SetFlags(uint32_t flags)              { mFlags = flags; }
    void AddFlag(uint32_t flag)                { mFlags |= flag; }
    void RemoveFlag(uint32_t flag)             { mFlags &= ~flag; }
    bool HasFlag(uint32_t flag) const          { return (mFlags & flag) != 0; }
    bool HasAnyFlag(uint32_t flags) const      { return (mFlags & flags) != 0; }
    uint32_t GetFlags() const                  { return mFlags; }
    
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
    
    // 有効/無効（「表示されているかどうか」という名だが、実質オン/オフフラグ）
    bool GetEnabled() const { return mEnabled; }
    void SetEnabled(bool b) { mEnabled = b; }
    
    // レイを取得（レイコライダー用に派生クラスで override する）
    virtual Ray GetRay() const { return Ray(); }
    
private:
    // 少なくとも 1 つ以上のコライダーと当たっているか
    bool mIsCollided;
    
    // 判定を行うかどうかのフラグ（「見えている」的な意味合いも含む）
    bool mEnabled;
    
    // 自動で生成されたバウンディングボリューム
    class BoundingVolumeComponent* mBoundingVolume;
    
    // 自分のコライダー種別（ビットフラグ）
    uint32_t mFlags;
    
    // このフレーム中に衝突した相手の一覧
    std::vector<ColliderComponent*> mTargetColliders;
};

} // namespace toy
