#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"

#include <memory>

namespace toy {

class Texture;
class RenderQueue;

//==================================================
// NineSliceSpriteComponent
//==================================================
// ・UI 用の 9-slice スプライト（ウィンドウ枠・ボタン背景など）
// ・テクスチャを border で 3x3 に分け、
//     四隅：伸ばさない / 上下の辺：横だけ / 左右の辺：縦だけ / 中央：縦横
//   に伸ばして SetSize() の大きさに描く
// ・座標は SpriteComponent と同じ UI 論理座標（左上原点）
//   描画位置 = Actor 位置 + Offset、そこから右下へ Size
//
// 使い方
//   auto* frame = CreateComponent<NineSliceSpriteComponent>(900);
//   frame->SetTexture(assets->GetRoundedRectTexture(8));
//   frame->SetBorder(8);          // テクスチャ上の角の大きさ（px）
//   frame->SetSize(rect.size);    // 描画サイズ（論理 px）
//==================================================
class NineSliceSpriteComponent : public VisualComponent
{
public:
    NineSliceSpriteComponent(class Actor* a, int drawOrder = 100,
                             VisualLayer layer = VisualLayer::UI);
    ~NineSliceSpriteComponent() override = default;

    void GatherRenderItems(RenderQueue& out) override;

    void SetTexture(std::shared_ptr<Texture> tex) override;

    //--------------------------------------------------------------------------
    // 分割位置（テクスチャ上の px）
    //--------------------------------------------------------------------------
    void SetBorder(int px) { SetBorder(px, px, px, px); }
    void SetBorder(int left, int top, int right, int bottom);

    // 描画時の角の大きさ = border * cornerScale（既定 1 = テクスチャの px のまま）
    void SetCornerScale(float s) { mCornerScale = s; }

    //--------------------------------------------------------------------------
    // 描画サイズ・位置（論理 px）
    //--------------------------------------------------------------------------
    void SetSize(const Vector2& size) { mSize = size; }
    const Vector2& GetSize() const { return mSize; }

    void SetOffset(const Vector3& o) { mOffset = o; }
    const Vector3& GetOffset() const { return mOffset; }

    //--------------------------------------------------------------------------
    // 色・アルファ
    //--------------------------------------------------------------------------
    void SetColor(const Vector3& color) { mColor = color; }
    const Vector3& GetColor() const { return mColor; }

    void SetAlpha(float a) { mAlpha = Math::Clamp(a, 0.0f, 1.0f); }
    float GetAlpha() const { return mAlpha; }

private:
    int   mTexWidth  { 0 };
    int   mTexHeight { 0 };

    // left, top, right, bottom（テクスチャ px）
    int   mBorder[4] { 0, 0, 0, 0 };
    float mCornerScale { 1.0f };

    Vector2 mSize   { Vector2::Zero };
    Vector3 mOffset { Vector3::Zero };

    Vector3 mColor { 1.0f, 1.0f, 1.0f };
    float   mAlpha { 1.0f };
};

} // namespace toy
