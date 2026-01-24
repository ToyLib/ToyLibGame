#include "Movement/MoveComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"
#include "Physics/ColliderComponent.h"

namespace toy {

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
// ・各種速度を 0 で初期化
// ・mIsMovable は true（移動可能）で開始
MoveComponent::MoveComponent(class Actor* a, int updateOrder)
    : Component(a, updateOrder)
    , mAngularSpeed(0.0f)
    , mForwardSpeed(0.0f)
    , mRightSpeed(0.0f)
    , mVerticalSpeed(0.0f)
    , mIsMovable(true)
{
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
// ・毎フレーム、設定された速度に応じて Actor の回転・位置を更新する。
// ・ここでは「単純な移動」のみを担当し、壁判定付きの移動は
//   TryMoveWithRayCheck() 側に任せる想定。
//------------------------------------------------------------------------------
void MoveComponent::Update(float deltaTime)
{
    // 現在の回転を取得
    Quaternion rot = GetOwner()->GetRotation();

    // --- 回転（ヨー軸） ---
    // mAngularSpeed は「度/秒」を想定し、ここでラジアンに変換して使用。
    if (!Math::NearZero(mAngularSpeed))
    {
        float angle = Math::ToRadians(mAngularSpeed * deltaTime);
        Quaternion inc(Vector3::UnitY, angle);
        rot = Quaternion::Concatenate(rot, inc);
        GetOwner()->SetRotation(rot);
    }

    // --- 位置更新 ---
    Vector3 pos = GetOwner()->GetPosition();

    // 前後移動（ローカル前方向）
    if (!Math::NearZero(mForwardSpeed))
    {
        pos += GetOwner()->GetForward() * mForwardSpeed * deltaTime;
    }
    // 左右ストレイフ（ローカル右方向）
    if (!Math::NearZero(mRightSpeed))
    {
        pos += GetOwner()->GetRight() * mRightSpeed * deltaTime;
    }
    // 上下移動（ローカル上方向）
    if (!Math::NearZero(mVerticalSpeed))
    {
        pos += GetOwner()->GetUpward() * mVerticalSpeed * deltaTime;
    }

    GetOwner()->SetPosition(pos);
}

//------------------------------------------------------------------------------
// Reset
//------------------------------------------------------------------------------
// ・すべての速度を 0 に戻す。
// ・「アニメ中は停止」「ダウン中は移動禁止」などの切り替え時に使用。
//------------------------------------------------------------------------------
void MoveComponent::Reset()
{
    mAngularSpeed  = 0.0f;
    mForwardSpeed  = 0.0f;
    mRightSpeed    = 0.0f;
    mVerticalSpeed = 0.0f;
}

//------------------------------------------------------------------------------
// TryMoveWithRayCheck
//------------------------------------------------------------------------------
// 壁すり抜け防止付きの移動処理。
//
// moveVec   : フレームあたりの移動ベクトル（速度ベース想定）
// deltaTime : 経過時間
//
// 処理の流れ：
//  1. 現在位置から「理想的な移動先」まで Ray を飛ばす。
//  2. Ray が壁に当たった場合は、衝突点(stopPos) までに位置を制限。
//  3. 何も当たらなければ、そのまま goal まで移動。
//  4. 念のため CollideAndCallback(C_PLAYER, C_WALL, ...) を呼び、
//     OBB などの MTV 押し戻しを実行して最終的なめり込みを防ぐ。
//
// 戻り値：現状は常に true（呼び出し成功）
//------------------------------------------------------------------------------
bool MoveComponent::TryMoveWithRayCheck(const Vector3& moveVec, float deltaTime)
{
    if (!GetOwner() || !mIsMovable) return false;

    Actor* owner = GetOwner();
    PhysWorld* phys = owner->GetApp()->GetPhysWorld();

    const Vector3 start = owner->GetPosition();
    const Vector3 goal  = start + moveVec * deltaTime;

    // ほぼ無移動なら終わり
    if ((goal - start).LengthSq() < Math::NearZeroEpsilon)
    {
        return true;
    }

    //========================================================
    // ★壁チェックRayを複数高さで撃つ（低い壁/床際の壁対策）
    //========================================================
    // キャラの当たりの高さ感に合わせて調整（まずはこの2本でOK）
    const float kRayLow  = 0.15f;  // 足元より少し上（床の側面を拾いすぎない程度）
    const float kRayMid  = 0.60f;  // 腰〜胸

    Vector3 bestStop = goal;
    bool hitAny = false;

    auto CastAtHeight = [&](float h, bool filterFloorLike) -> void
    {
        Vector3 s = start; s.y += h;
        Vector3 g = goal;  g.y += h;

        Vector3 stop, n;

        // 45度までを床扱い（調整したいなら 0.8f(≈36°) とかでもOK）
        constexpr float kCosFloorLike = 0.707f;

        const float cosFloorLike = filterFloorLike ? kCosFloorLike : 0.0f;

        if (phys->RayHitWallEx(s, g, C_WALL, /*ignoreActor*/owner,
                               cosFloorLike, stop, n))
        {
            stop.y = goal.y;

            const float curDistSq = (bestStop - start).LengthSq();
            const float newDistSq = (stop    - start).LengthSq();
            if (!hitAny || newDistSq < curDistSq)
            {
                bestStop = stop;
            }
            hitAny = true;
        }
    };

    CastAtHeight(kRayMid, false);
    CastAtHeight(kRayLow, true); 
    owner->SetPosition(hitAny ? bestStop : goal);

    //========================================================
    // ★MTV押し戻しは“最後の保険”として弱めに一回だけ
    //   （床がWALL兼用だと詰まりやすいので、押し戻し量が大きい時だけにする）
    //========================================================
    //const Vector3 before = owner->GetPosition();
    phys->CollideAndCallback(C_PLAYER_TEAM, C_WALL, true, false);
    //const Vector3 after = owner->GetPosition();

    // 押し戻しが大きすぎる（=床側面を壁扱いしてる）なら戻す、なども可能
    // まずはログで after-before を見て判断してもOK

    return true;
}
} // namespace toy
