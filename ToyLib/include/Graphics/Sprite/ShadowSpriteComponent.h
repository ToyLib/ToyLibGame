#pragma once
#include "Graphics/Sprite/GroundConformSpriteComponent.h"

namespace toy {

//==============================================================================
// ShadowSpriteComponent
//  - GroundConformSpriteComponent の派生：簡易シャドウ（足元影）用
//  - 地面に沿う（Terrain/Collider床）＋ライト方向に合わせて伸ばす
//
// ポイント（旧互換）
//  - シェーダは Sprite を使用（uSpriteColor/uSpriteAlpha）
//  - サイズはワールド単位（SetSize）
//  - 伸ばしは “グリッド生成時に” 反映（GroundConformは頂点がワールドだから）
//==============================================================================
class ShadowSpriteComponent : public GroundConformSpriteComponent
{
public:
    ShadowSpriteComponent(class Actor* owner, int drawOrder = 10);
    ~ShadowSpriteComponent() override = default;

    // 影の伸び具合（奥行方向のスケール倍率）
    void SetStretch(float s) { mStretch = s; }               // 例：3.0f
    void SetAutoRotateByLight(bool on) { mAutoRotateByLight = on; }

protected:
    // GroundConform では BuildWorldMatrix は Identity のまま（頂点がワールド）
    Matrix4 BuildWorldMatrix() const override;

    // グリッド再構築時に yaw/伸ばし を反映したいので PreDraw を上書き
    void PreDraw() override;

    // 新パス
    void GatherRenderItems(class RenderQueue& queue) override;

private:
    // ライト方向から yaw を決める（XZ平面のみ）
    float ComputeLightYawRad() const;

private:
    float mStretch { 3.0f };
    bool  mAutoRotateByLight { true };
};

} // namespace toy
