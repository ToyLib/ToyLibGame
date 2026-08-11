#include "Movement/FollowMoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
FollowMoveComponent::FollowMoveComponent(Actor* owner, int updateOrder)
    : MoveComponent(owner, updateOrder)
{
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// 1. ベースの MoveComponent::Update() で通常の移動処理（※必要なら）
// 2. ターゲットとの方向 / 距離を計算
// 3. 前方ベクトルを toTarget に寄せるよう Y 回転（最大 mRotationSpeed）
// 4. 一定距離以上離れていれば、前方方向に向かって前進
//    ※ 実際の移動は TryMoveWithRayCheck() を使用し、壁すり抜けを防止
//------------------------------------------------------------------------------
void FollowMoveComponent::Update(float deltaTime)
{
    // 通常の MoveComponent の移動処理（Angular/Forward/Right/Vertical）
    MoveComponent::Update(deltaTime);
    
    // 追従対象が無いなら何もしない
    if (!mTarget)
    {
        return;
    }
    
    //============================================================
    // 1) ターゲット方向と距離
    //============================================================
    Vector3 toTarget = mTarget->GetPosition() - GetOwner()->GetPosition();
    const float dist = toTarget.Length();
    
    if (dist <= Math::NearZeroEpsilon)
    {
        return;
    }
    
    toTarget.Normalize();
    
    //============================================================
    // 2) 向き調整（Yaw のみ）
    //============================================================
    Vector3 forward = GetOwner()->GetForward();
    forward.y = 0.0f;

    Vector3 targetDir = toTarget;
    targetDir.y = 0.0f;

    if (forward.LengthSq() > Math::NearZeroEpsilon &&
        targetDir.LengthSq() > Math::NearZeroEpsilon)
    {
        forward.Normalize();
        targetDir.Normalize();

        const float signedAngle = Math::Atan2(
            Vector3::Cross(forward, targetDir).y,
            Vector3::Dot(forward, targetDir)
        );

        if (Math::Abs(signedAngle) > Math::ToRadians(1.0f))
        {
            const float maxRot =
                Math::ToRadians(mRotationSpeed) * deltaTime;

            const float yaw =
                Math::Clamp(signedAngle, -maxRot, maxRot);

            Quaternion rot = GetOwner()->GetRotation();
            const Quaternion inc(Vector3::UnitY, yaw);

            rot = Quaternion::Concatenate(rot, inc);
            GetOwner()->SetRotation(rot);
        }
    }
    
    //============================================================
    // 3) 距離が離れていたら前進（壁判定付き）
    //============================================================
    if (dist > mFollowDistance)
    {
        Vector3 moveDir = GetOwner()->GetForward();
        moveDir.y = 0.0f;
        moveDir.Normalize();
        
        // NOTE: 負号付きで使っているのは既存挙動を維持するため
        TryMoveWithRayCheck(moveDir * (mFollowSpeed), deltaTime);
    }
}

} // namespace toy
