#pragma once
#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>
#include <string>

namespace toy {

class GravityComponent;

//==============================================================================
// FootSpriteComponent
//  - Actor の足元に「地面に貼り付く板ポリ（スプライト）」を描く
//  - 影（簡易シャドウ） / ロックオンリング / 範囲表示 / 足元エフェクト等の基底
//
// 両対応（ここが今回の追加）
//  - SnapToGround  : 地面高さにスナップ（めり込み防止）
//  - AlignToGround : 地面法線に沿って傾ける（斜面追従）
//
// 前提
//  - GravityComponent がある Actor のみ「地面情報」を取得して追従する
//  - Gravity が無い Actor は従来どおり “Ownerの位置” を使って描画
//==============================================================================
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
    //--------------------------------------------------------------------------
    void SetTint(const Vector3& c) { mTint = c; }
    void SetAlpha(float a) { mAlpha = a; }
    void SetDiffuseColor(const Vector3& c) { mDiffuseColor = c; }

    //--------------------------------------------------------------------------
    // Ground follow options（★今回追加）
    //--------------------------------------------------------------------------
    // 地面高さにスナップ（めり込み防止）
    void SetSnapToGround(bool on) { mSnapToGround = on; }
    bool GetSnapToGround() const  { return mSnapToGround; }

    // 地面法線に沿って傾ける（斜面追従）
    void SetAlignToGround(bool on) { mAlignToGround = on; }
    bool GetAlignToGround() const  { return mAlignToGround; }

    // 少し浮かせる（Z-fighting/めり込み対策）
    void SetGroundLift(float y) { mGroundLift = y; }
    float GetGroundLift() const { return mGroundLift; }

    // raw / smooth のどちらを使うか（GravityのGroundPoseキャッシュを使用）
    //  - raw    : 即応（影向け）
    //  - smooth : 見た目が滑らか（車/4本足向け）
    void SetUseSmoothGroundPose(bool on) { mUseSmoothGroundPose = on; }
    bool GetUseSmoothGroundPose() const  { return mUseSmoothGroundPose; }

protected:
    //--------------------------------------------------------------------------
    // World matrix builder
    //--------------------------------------------------------------------------
    virtual Matrix4 BuildWorldMatrix() const;

    //--------------------------------------------------------------------------
    // Pre/Post draw hooks
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
    // Ground follow params（★今回追加）
    //--------------------------------------------------------------------------
    bool  mSnapToGround        = true;   // default: ON（めり込み対策）
    bool  mAlignToGround       = false;  // default: OFF（リングは水平の方が見やすい）
    bool  mUseSmoothGroundPose = false;  // default: raw（影はrawが自然）
    float mGroundLift          = 0.02f;  // default: 少し浮かせる

    //--------------------------------------------------------------------------
    // Unlit parameters
    //--------------------------------------------------------------------------
    Vector3 mTint         = Vector3(1.0f, 1.0f, 1.0f);
    float   mAlpha        = 1.0f;
    Vector3 mDiffuseColor = Vector3(1.0f, 1.0f, 1.0f);
};

} // namespace toy
