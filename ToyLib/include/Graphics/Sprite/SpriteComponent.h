#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

//==================================================
// SpriteComponent
//==================================================
// 2Dスプライトを画面に描画するコンポーネント。
// ・UIや2D HUD 表示向け（デフォルトで VisualLayer::UI）
// ・Texture を貼った矩形を画面空間で描画する
// ・スケーリングや画面左上固定描画などに対応
//==================================================
class SpriteComponent : public VisualComponent
{
public:
    // --------------------------------------------------------------
    // コンストラクタ
    //
    // drawOrder : 描画順（小さいほど先に描画）
    // layer     : UI / Overlay / Object3D などの描画レイヤー
    // --------------------------------------------------------------
    SpriteComponent(class Actor* a, int drawOrder,
                    VisualLayer layer = VisualLayer::UI);

    ~SpriteComponent();

    //==================================================
    // 描画処理 (Renderer が呼び出す)
    //==================================================
    void Draw() override;

    //==================================================
    // スプライトの幅・高さスケール設定
    //   w: 幅方向のスケール
    //   h: 高さ方向のスケール
    //==================================================
    void SetScale(float w, float h)
    {
        mScaleWidth  = w;
        mScaleHeight = h;
    }

    //==================================================
    // 使用するテクスチャを設定
    //==================================================
    void SetTexture(std::shared_ptr<class Texture> tex) override;

    //==================================================
    // 左上固定フラグ
    //
    // true  → (0,0) を画面左上としてスプライトを描画
    // false → Actor ワールド座標を ViewProj で変換して描画
    //
    // UI 用スプライトでは通常 true
    //==================================================
    void SetIsTopLeft(bool b) { mIsTopLeft = b; }
    
    // 色（RGB）
    void SetColor(const Vector3& color) { mColor = color; }
    const Vector3& GetColor() const { return mColor; }

    // 透明度（0.0〜1.0）
    void SetAlpha(float a) { mAlpha = Math::Clamp(a, 0.0f, 1.0f); }
    float GetAlpha() const { return mAlpha; }


private:
    //==================================================
    // パラメータ
    //==================================================

    // スケール（幅／高さ）
    float mScaleWidth;   // X方向の拡大率
    float mScaleHeight;  // Y方向の拡大率

    // テクスチャサイズ
    int   mTexWidth;     // ピクセル幅
    float mTexHeight;    // ピクセル高さ

    // 現在の画面サイズ（UI の場合に使用）
    int   mScreenWidth;
    int   mScreenHeight;

    // 左上固定（true のとき Actor 位置ではなく画面座標で描画）
    bool  mIsTopLeft;
    
    // カラー属性
    Vector3 mColor;     // RGB
    float mAlpha;       // A
};

} // namespace toy
