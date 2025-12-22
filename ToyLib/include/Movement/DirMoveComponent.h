#pragma once
#include "Movement/MoveComponent.h"

namespace toy {

//------------------------------------------------------------------------------
// DirMoveComponent
//------------------------------------------------------------------------------
// ・カメラ方向（ビュー基準）に対して移動する MoveComponent の派生。
// ・WASD / 左スティック入力を "カメラの向き" に合わせて前後左右へ移動する。
// ・移動後は進行方向へ自然に回転する（AdjustDir）。
//------------------------------------------------------------------------------
class DirMoveComponent : public MoveComponent
{
public:
    DirMoveComponent(class Actor* owner, int updateOrder = 10);
    virtual ~DirMoveComponent();

    // メイン更新（カメラ基準移動 → 衝突付き移動 → 向き調整）
    void Update(float deltaTime) override;

    // 入力受付（左スティック・DPad を速度に反映）
    void ProcessInput(const struct InputState& state) override;

private:
    // カメラ基準の移動速度（前後左右の最大値）
    float mSpeed;

    // 前フレーム位置（回転の補助用）
    Vector3 mPrevPosition;

    // 実際に動いた方向へ Actor を向ける
    void AdjustDir();
    
    // ユーザーインプットによる自発的な動きか
    bool mDidMoveByInput;
};

} // namespace toy
