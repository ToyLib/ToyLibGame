//==============================================================================
// FootSpriteComponent
//  - Actor の足元に「地面に貼り付く板ポリ（スプライト）」を描く
//  - 影（簡易シャドウ） / ロックオンリング / 範囲表示 / 足元エフェクト等の基底
//
// 描画の特徴
//  - 3D空間上の板ポリ（Effect3D想定）
//  - Sprite用のQuad（XY平面）を使い、X回転で地面（XZ）に寝かせる
//  - Unlit シェーダ（Phong互換uniformあり）で描く前提
//
// 重要なポイント（Unlit互換）
//  - uUseTint を FootSprite 側で必ず 1 にする：
//      TextBillboard 等、同じ Unlit を使う別コンポーネントの「互換モード」を壊さないため。
//  - uDiffuseColor は「テクスチャ無し運用」用の保険（将来用）
//==============================================================================

#pragma once
#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>
#include <string>

namespace toy {

class FootSpriteComponent : public VisualComponent
{
public:
    FootSpriteComponent(class Actor* owner,
                        int drawOrder = 10,
                        VisualLayer layer = VisualLayer::Effect3D);

    virtual ~FootSpriteComponent() = default;

    //--------------------------------------------------------------------------
    // Draw
    //--------------------------------------------------------------------------
    void Draw() override;

    //--------------------------------------------------------------------------
    // Texture (VisualComponent互換)
    //--------------------------------------------------------------------------
    void SetTexture(std::shared_ptr<class Texture> tex) override;
    std::shared_ptr<Texture> GetTexture() const { return mTexture; }

    //--------------------------------------------------------------------------
    // Size (world units)
    //  - width : X 方向の幅
    //  - depth : Z 方向の奥行（※内部的にはYスケールに入る。寝かせるため）
    //--------------------------------------------------------------------------
    void SetSize(float width, float depth) { mWidth = width; mDepth = depth; }
    void SetWidth(float w) { mWidth = w; }
    void SetDepth(float d) { mDepth = d; }

    //--------------------------------------------------------------------------
    // Offset (足元基準)
    //--------------------------------------------------------------------------
    void SetOffsetPosition(const Vector3& v) { mOffsetPosition = v; }

    //--------------------------------------------------------------------------
    // Additional scale (演出用)
    //--------------------------------------------------------------------------
    void SetOffsetScale(float s) { mOffsetScale = s; }

    //--------------------------------------------------------------------------
    // Rotation on ground (Yaw)
    //--------------------------------------------------------------------------
    void SetYaw(float rad) { mYaw = rad; }

    //--------------------------------------------------------------------------
    // Unlit parameters
    //  - Tint/Alpha : ロックオンリングや影の濃さ調整などに使う
    //  - DiffuseColor : テクスチャ無し運用用（色だけ出したい時）
    //--------------------------------------------------------------------------
    void SetTint(const Vector3& c) { mTint = c; }
    void SetAlpha(float a) { mAlpha = a; }
    void SetDiffuseColor(const Vector3& c) { mDiffuseColor = c; }

protected:
    //--------------------------------------------------------------------------
    // World matrix builder
    //  - 派生クラスが「変形の仕方だけ」変えたい場合に override する
    //--------------------------------------------------------------------------
    virtual Matrix4 BuildWorldMatrix() const;

    //--------------------------------------------------------------------------
    // Pre/Post draw hooks
    //  - 派生側で uYaw を計算する、アニメ的に OffsetScale を揺らす等
    //--------------------------------------------------------------------------
    virtual void PreDraw()  {}
    virtual void PostDraw() {}

protected:
    std::shared_ptr<class Texture> mTexture;

    //--------------------------------------------------------------------------
    // Transform parameters (world units)
    //--------------------------------------------------------------------------
    float   mWidth  = 1.0f;
    float   mDepth  = 1.0f;

    Vector3 mOffsetPosition = Vector3::Zero;
    float   mOffsetScale    = 1.0f;
    float   mYaw            = 0.0f;

    //--------------------------------------------------------------------------
    // Unlit parameters
    //--------------------------------------------------------------------------
    Vector3 mTint         = Vector3(1.0f, 1.0f, 1.0f);
    float   mAlpha        = 1.0f;
    Vector3 mDiffuseColor = Vector3(1.0f, 1.0f, 1.0f);
};

} // namespace toy
