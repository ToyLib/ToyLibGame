#pragma once
#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

/**
 * ------------------------------------------------------------
 * ShadowSpriteComponent
 * ------------------------------------------------------------
 * ・キャラクターの足元に投影される「影スプライト」を描くコンポーネント
 * ・単純な平面スプライト（円形影など）を Actor の位置に追従して描画
 * ・地面の影は ShadowMapping とは別で、見やすさや演出用の簡易影
 * ------------------------------------------------------------
 */
class ShadowSpriteComponent : public VisualComponent
{
public:
    // --------------------------------------------------------
    // コンストラクタ / デストラクタ
    // --------------------------------------------------------
    ShadowSpriteComponent(class Actor* owner, int drawOrder = 10);
    ~ShadowSpriteComponent();
    
    // --------------------------------------------------------
    // 描画（Actor の足元にスプライト影を描画）
    // --------------------------------------------------------
    void Draw() override;
    
    // --------------------------------------------------------
    // テクスチャ設定（影画像）
    // --------------------------------------------------------
    void SetTexture(std::shared_ptr<class Texture> tex) override;
    
    // --------------------------------------------------------
    // 影の大きさ（幅・高さ）
    // --------------------------------------------------------
    void SetScale(float width, float height)
    {
        mScaleWidth  = width;
        mScaleHeight = height;
    }
    
    // --------------------------------------------------------
    // スプライトの位置・スケール補正
    //  - OffsetPosition : Actor の足元からのずれ（XZ オフセット用途）
    //  - OffsetScale    : 動きやアニメに合わせて影サイズを可変にしたいとき
    // --------------------------------------------------------
    void SetOffsetPosition(const Vector3& vPos) { mOffsetPosition = vPos; }
    void SetOffsetScale(const float f)          { mOffsetScale    = f; }
    
private:
    // 描画に使用するテクスチャ（丸影など）
    std::shared_ptr<class Texture> mTexture;
    
    // スプライト影の基準スケール
    float mScaleWidth;
    float mScaleHeight;
    
    // 位置補正（XZ 中心位置を微調整）
    Vector3 mOffsetPosition;
    
    // 影のスケール補正係数
    float mOffsetScale;
};

} // namespace toy
