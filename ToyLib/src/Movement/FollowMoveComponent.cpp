#include "Movement/FollowMoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
FollowMoveComponent::FollowMoveComponent(Actor* owner, int updateOrder)
    : MoveComponent(owner, updateOrder)
    , mTarget(nullptr)
    , mFollowDistance(3.0f)   // これ以上離れたら追いかける
    , mFollowSpeed(200.0f)    // 追従速度
    , mRotationSpeed(90.0f)   // 1秒あたりの最大回転角（度）
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
    // 2) 向き調整（Yaw のみ、最大 mRotationSpeed）
    //============================================================
    const Vector3 forward = GetOwner()->GetForward();
    
    float dot = Vector3::Dot(forward, toTarget);
    dot = Math::Clamp(dot, -1.0f, 1.0f);
    
    float angle = Math::Acos(dot); // 0〜π
    
    // 1度未満ならほぼ合っているので回転しない
    if (angle > Math::ToRadians(1.0f))
    {
        // このフレームで回せる最大角度（ラジアン）
        const float maxRot = Math::ToRadians(mRotationSpeed) * deltaTime;
        angle = Math::Min(angle, maxRot);
        
        // XZ 平面上で符号付き角度を取得
        Vector3 f2D(forward.x, 0.0f, forward.z);
        Vector3 t2D(toTarget.x, 0.0f, toTarget.z);
        f2D.Normalize();
        t2D.Normalize();
        
        const float signedAngle = Math::Atan2(
                                              Vector3::Cross(f2D, t2D).y,
                                              Vector3::Dot(f2D, t2D)
                                              );
        
        // 実際に回す角度を [-angle, +angle] に制限
        const float yaw = Math::Clamp(signedAngle, -angle, angle);
        
        Quaternion rot = GetOwner()->GetRotation();
        const Quaternion inc(Vector3::UnitY, yaw);
        rot = Quaternion::Concatenate(rot, inc);
        GetOwner()->SetRotation(rot);
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
        TryMoveWithRayCheck(moveDir * (-mFollowSpeed), deltaTime);
    }
}

} // namespace toy
