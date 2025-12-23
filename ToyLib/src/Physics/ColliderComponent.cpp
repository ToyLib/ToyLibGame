#include "Physics/ColliderComponent.h"
#include "Engine/Core/Actor.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"

#include <algorithm>

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
// ・自分用の BoundingVolumeComponent を自動で生成して Actor に付与。
// ・PhysWorld に自分自身を登録し、衝突判定の対象にする。
//------------------------------------------------------------------------------
ColliderComponent::ColliderComponent(Actor* a)
    : Component(a)
    , mFlags(C_NONE)
    , mIsCollided(false)
    , mEnabled(true)
    , mIsTrigger(false)
{
    // 当たり判定形状（AABB/OBB/Polygon）を持つコンポーネントを自動生成
    mBoundingVolume = GetOwner()->CreateComponent<BoundingVolumeComponent>();
    
    // 物理ワールドへ登録
    GetOwner()->GetApp()->GetPhysWorld()->AddCollider(this);
}

//------------------------------------------------------------------------------
// デストラクタ
//------------------------------------------------------------------------------
// ・PhysWorld から自分を除外。
//------------------------------------------------------------------------------
ColliderComponent::~ColliderComponent()
{
    GetOwner()->GetApp()->GetPhysWorld()->RemoveCollider(this);
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・現状は何もしていないが、将来「自前で毎フレーム何か更新」したくなったとき用。
// ・衝突バッファのクリアは PhysWorld 側で行う設計。
//------------------------------------------------------------------------------
void ColliderComponent::Update(float deltaTime)
{
    // mTargetColliders.clear(); // クリアは PhysWorld::Test() 側で実施
}

//------------------------------------------------------------------------------
// Collided
//------------------------------------------------------------------------------
// ・PhysWorld 側から「このコライダーと当たった」と通知されるエントリポイント。
// ・同一コライダーが二重登録されないようにチェックしてから追加する。
// ・何か 1 件でも追加されたら mIsCollided を true にする。
//------------------------------------------------------------------------------
void ColliderComponent::Collided(ColliderComponent* c)
{
    if (std::find(mTargetColliders.begin(), mTargetColliders.end(), c) == mTargetColliders.end())
    {
        mTargetColliders.emplace_back(c);
        mIsCollided = true;
    }
}

} // namespace toy
