//==============================================================================
// ShadowSpriteComponent
//  - FootSpriteComponent の派生：簡易シャドウ（足元影）用
//  - 地面に貼り付く板ポリを、ライト方向に合わせて伸ばして描く
//
// ポイント
//  - 見た目を昔の実装に寄せるため、シェーダは Sprite を使用する
//    → そのため FootSpriteComponent::Draw() は使わず、Draw() を override して
//      Sprite 用 uniform（uSpriteColor/uSpriteAlpha）をセットする
//  - サイズはワールド単位（SetSize）で制御（テクスチャのpx依存にしない）
//
// 依存
//  - LightingManager::GetLightDirection() でライト方向を取得してYawを決める
//==============================================================================

#pragma once
#include "Graphics/Sprite/FootSpriteComponent.h"

namespace toy {

class ShadowSpriteComponent : public FootSpriteComponent
{
public:
    ShadowSpriteComponent(class Actor* owner, int drawOrder = 10);
    ~ShadowSpriteComponent() override = default;

    // 影の伸び具合（奥行方向のスケール倍率）
    void SetStretch(float s) { mStretch = s; }               // 例：3.0f
    void SetAutoRotateByLight(bool on) { mAutoRotateByLight = on; }

    // Sprite シェーダ用に Draw を上書き（FootSpriteのUnlit uniform送信を避ける）
    void Draw() override;

protected:
    // 影用のワールド行列（Yawの自動計算＋奥行方向の伸ばし）
    Matrix4 BuildWorldMatrix() const override;

private:
    float mStretch = 3.0f;
    bool  mAutoRotateByLight = true;
};

} // namespace toy
