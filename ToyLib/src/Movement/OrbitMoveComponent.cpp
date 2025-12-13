#include "Movement/OrbitMoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
OrbitMoveComponent::OrbitMoveComponent(class Actor* owner, int updateOrder)
    : MoveComponent(owner, updateOrder)
    , mCenterActor(nullptr)
    , mOrbitRadius(5.0f)
    , mOrbitSpeed(Math::ToRadians(45.0f))  // デフォルト：45°/s
    , mCurrentAngle(0.0f)
{
}

//------------------------------------------------------------------------------
// Update
// ・角度を進める
// ・中心アクターの周囲を円運動させる
// ・Yは対象アクターの現在高さを維持する（地面追従等は外側で制御）
//------------------------------------------------------------------------------
void OrbitMoveComponent::Update(float deltaTime)
{
    // 公転対象がいない場合は何もしない
    if (!mCenterActor)
    {
        MoveComponent::Update(deltaTime);
        return;
    }
    
    // 角度を加算（時計回り/反時計回りは符号で調整）
    mCurrentAngle += mOrbitSpeed * deltaTime;
    
    // 角度 → 位置（XZ 平面の円運動）
    float x = mCenterActor->GetPosition().x + mOrbitRadius * Math::Cos(mCurrentAngle);
    float z = mCenterActor->GetPosition().z + mOrbitRadius * Math::Sin(mCurrentAngle);
    
    // Y はアクター自身の高さを維持
    float y = GetOwner()->GetPosition().y;
    
    // 最終位置適用
    GetOwner()->SetPosition(Vector3(x, y, z));
    
    // MoveComponent 側の処理（回転など）があれば実行
    MoveComponent::Update(deltaTime);
}

} // namespace toy
