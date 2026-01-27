#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"

namespace toy {

//==============================================================
// MoveComponent
//
// アクターの移動・回転を行う基本コンポーネント。
//
// ・Actor::Update() の中で Update() が呼ばれ、速度に応じて位置更新。
// ・派生クラス（FollowMoveComponent, OrbitMoveComponent, FPSMoveComponent など）
//   で入力処理や挙動を上書きしやすいよう、速度パラメータを持つ。
// ・壁すり抜け防止（TryMoveWithRayCheck）にも対応。
//==============================================================
class MoveComponent : public Component
{
public:
    MoveComponent(class Actor* owner, int updateOrder = 10);

    //==============================
    // 毎フレーム更新
    //==============================
    // ・角速度 → 回転更新
    // ・前後/左右/上下 → 位置更新
    // ・壁のレイ判定つき移動もサポート
    void Update(float deltaTime) override;

    //==============================
    // Getter / Setter
    //==============================
    float GetAngularSpeed()   const { return mAngularSpeed;   }
    float GetForwardSpeed()   const { return mForwardSpeed;   }
    float GetRightSpeed()     const { return mRightSpeed;     }
    float GetVerticalSpeed()  const { return mVerticalSpeed;  }

    void SetAngularSpeed(float speed)  { mAngularSpeed = speed; }
    void SetForwardSpeed(float speed)  { mForwardSpeed = speed; }
    void SetRightSpeed(float speed)    { mRightSpeed   = speed; }
    void SetVerticalSpeed(float speed) { mVerticalSpeed = speed; }

    //==============================
    // 移動可能 ／ 回転可能
    //==============================
    // ・IsMovable を false にすると移動入力をロック
    // ・ロックした瞬間に速度をクリア（意図せぬ慣性を防止）
    void SetIsMovable(const bool b) { mIsMovable = b; if (!b) Reset(); }
    bool GetIsMovable() const { return mIsMovable; }

    void SetIsTurnable(const bool b) { mIsTurnable = b; }
    bool GetIsTurnable() const { return mIsTurnable; }

    //==============================
    // 移動量リセット
    //==============================
    // ・アニメ中の移動制御や、特殊状態（ダウン、攻撃硬直）で利用。
    void Reset();

    //==============================
    // 壁すり抜け防止移動
    //==============================
    // moveVec: 実際に移動したい方向・量
    // deltaTime: 時間
    //
    // ・Ray を飛ばして衝突した場合は移動量を制限
    // ・ジャンプ／落下中の処理とも併用可能
    // ・成功したら true、何かに当たって押し戻されたら false
    bool TryMoveWithRayCheck(const Vector3& moveVec, float deltaTime);

protected:
    //==============================
    // 移動パラメータ
    //==============================
    float mAngularSpeed  {};   // 回転速度（ヨー方向）
    float mForwardSpeed  {};   // 前後移動
    float mRightSpeed    {};   // 左右ストレイフ
    float mVerticalSpeed {};   // 上下（主に飛行や重力）

    //==============================
    // ステート管理
    //==============================
    bool mIsMovable  { true };  // 移動可能か（行動ロック時などに false）
    bool mIsTurnable { true };  // 回転可能か
};

} // namespace toy
