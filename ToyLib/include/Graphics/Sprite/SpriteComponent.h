#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"

#include <memory>

namespace toy {

class Texture;
class RenderQueue;

//==================================================
// SpriteComponent
//==================================================
// ・UI/2D向けスプライト
// ・描画は RenderQueue 経由（GatherRenderItems）
// ・Draw() は旧経路互換のため残すが空実装
//==================================================
class SpriteComponent : public VisualComponent
{
public:
    SpriteComponent(class Actor* a, int drawOrder = 100,
                    VisualLayer layer = VisualLayer::UI);

    ~SpriteComponent() override = default;

    // 新方式：RenderItem を積む
    void GatherRenderItems(RenderQueue& out) override;

    // サイズ倍率
    void SetScale(float w, float h)
    {
        mScaleWidth  = w;
        mScaleHeight = h;
    }

    // テクスチャ
    void SetTexture(std::shared_ptr<Texture> tex) override;

    // 左上固定（UI用）
    void SetIsTopLeft(bool b) { mIsTopLeft = b; }

    // 色・アルファ
    void SetColor(const Vector3& color) { mColor = color; }
    const Vector3& GetColor() const { return mColor; }

    void SetAlpha(float a) { mAlpha = Math::Clamp(a, 0.0f, 1.0f); }
    float GetAlpha() const { return mAlpha; }

    // 追加：ローカルオフセット（論理座標）
    void SetOffset(const Vector3& o) { mOffset = o; }
    const Vector3& GetOffset() const { return mOffset; }

private:
    // スケール（幅／高さ）
    float mScaleWidth  { 1.0f };
    float mScaleHeight { 1.0f };

    // テクスチャサイズ（ピクセル）
    int   mTexWidth  { 0 };
    int   mTexHeight { 0 };

    // 左上固定
    bool  mIsTopLeft { true };

    // カラー
    Vector3 mColor { 1.0f, 1.0f, 1.0f };
    float   mAlpha { 1.0f };

    Vector3 mOffset = Vector3::Zero;
};

} // namespace toy
