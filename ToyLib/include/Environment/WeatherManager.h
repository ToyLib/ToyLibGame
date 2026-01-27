#pragma once
#include "Environment/WeatherType.h"
#include "Environment/WeatherDomeComponent.h"
#include "Environment/WeatherOverlayComponent.h"
#include <memory>


namespace toy {

class WeatherManager
{
public:
    WeatherManager() = default;

    //==================================================================
    // Update
    //==================================================================
    // 天候の時間変化・遷移処理（必要であれば）
    // ※現状では最低限の更新のみ
    //==================================================================
    void Update(float deltaTime);

    // SkyDome（空・光源）側との連携
    void SetWeatherDome(class toy::WeatherDomeComponent* dome) { mWeatherDome = dome; }

    // Overlay（雨粒/雪/雷など）側との連携
    void SetWeatherOverlay(class toy::WeatherOverlayComponent* overlay) { mWeatherOverlay = overlay; }

    //==================================================================
    // ChangeWeather
    //==================================================================
    // 天候を変更する。
    // ・SkyDome に天候をセット → 空の色や昼夜光源が変わる
    // ・Overlay に天候をセット → パーティクル強度が変わる
    //
    // 基本は即時反映。将来的にはフェード処理もここに追加可能。
    //==================================================================
    void ChangeWeather(const toy::WeatherType weather);

private:
    // 画面エフェクト（雨・雪・雷など）
    class WeatherOverlayComponent* mWeatherOverlay { nullptr };

    // 空・光源・フォグなど
    class WeatherDomeComponent* mWeatherDome  { nullptr };

    // 現在の天気（基準状態）
    WeatherType mWeather { WeatherType::CLEAR };
};

} // namespace toy
