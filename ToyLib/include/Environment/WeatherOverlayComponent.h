#pragma once
#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

//======================================================================
// Overlay（画面全体の後処理系エフェクト）
// ・雨粒のスクリーンスペース描画
// ・霧のポストエフェクト
// ・雪のスクリーンスペース描画
//
// ※ WeatherManager から数値をセットされ、Draw() で強さに応じて描画。
// ※ SkyDome（空・太陽）とは独立しており、画面へのオーバーレイ担当。
//======================================================================
class WeatherOverlayComponent : public VisualComponent
{
public:
    WeatherOverlayComponent(class Actor* owner,
                            int drawOrder = 100,
                            VisualLayer layer = VisualLayer::OverlayScreen);

    // 画面全体のオーバーレイ描画
    void Draw() override;

    //------ 天候強度のセット（WeatherManager から渡される） ------
    void SetRainAmount (const float amt) { mRainAmount = amt; }
    void SetFogAmount  (const float amt) { mFogAmount  = amt; }
    void SetSnowAmout  (const float amt) { mSnowAmount = amt; }
    
    //------ 太陽/月の方向のセット（WeatherManager から渡される） ------
    void SetSunDir(const Vector3& dir)  { mSunDir = dir; }
    void SetMoonDir(const Vector3& dir) { mMoonDir = dir; }

private:
    //------ 各エフェクトの強度（0.0〜1.0） ------
    float mRainAmount;   // 雨（雨粒の量・密度）
    float mFogAmount;    // 霧（画面の白み・減衰）
    float mSnowAmount;   // 雪（雪粒の量・密度）


    // 太陽/月の向き
    Vector3 mSunDir;
    Vector3 mMoonDir;
};

} // namespace toy
