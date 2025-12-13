#include "Physics/LaserColliderComponent.h"
#include "Engine/Core/Actor.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
// ・ColliderComponent を継承しているが、形状（AABB・OBB）は使わない。
// ・C_LASER フラグをセットすることで、PhysWorld 側で「Ray vs Enemy」などの
//   特定組み合わせ判定に使われる。
//------------------------------------------------------------------------------
LaserColliderComponent::LaserColliderComponent(Actor* a)
    : ColliderComponent(a)
    , mLength(100.0f) // デフォルトの射程（描画用）
{
    // これはレーザー専用のコライダー
    SetFlags(C_LASER);
}

//------------------------------------------------------------------------------
// GetRay
//------------------------------------------------------------------------------
// ・レーザーの「発射位置」と「方向（前方ベクトル）」を Ray として返す。
// ・PhysWorld 側で Ray vs Mesh / Ray vs Polygon の衝突判定を行う。
// ・mLength は返さない（Ray は無限射程）→ 判定部分で必要に応じて制限可。
//------------------------------------------------------------------------------
Ray LaserColliderComponent::GetRay() const
{
    Vector3 start = GetOwner()->GetPosition();  // レーザーの発射点
    Vector3 dir   = GetOwner()->GetForward();   // Actor の前方方向に飛ばす
    return Ray(start, dir);
}

} // namespace toy
