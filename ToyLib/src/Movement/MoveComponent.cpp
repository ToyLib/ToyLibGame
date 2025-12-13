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
    // オーナーが無い or そもそも移動不可なら何もしない
    if (!GetOwner() || !mIsMovable) return false;

    // Ray のスタート／ゴール
    Vector3 start = GetOwner()->GetPosition();
    Vector3 goal  = start + moveVec * deltaTime;

    Vector3 stopPos;
    // 壁との Ray 判定
    if (GetOwner()->GetApp()->GetPhysWorld()->RayHitWall(start, goal, stopPos))
    {
        // 壁に当たった場合は、衝突直前まで移動
        GetOwner()->SetPosition(stopPos);
    }
    else
    {
        // 何も無ければそのままゴールへ
        GetOwner()->SetPosition(goal);
    }

    // 念のための押し戻し（MTV での補正）
    // C_PLAYER vs C_WALL：pushBack=true, callback=false
    GetOwner()->GetApp()->GetPhysWorld()->CollideAndCallback(C_PLAYER, C_WALL, true, false);

    return true;
}

} // namespace toy
