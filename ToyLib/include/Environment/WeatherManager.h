#pragma once

#include "Environment/WeatherTypes.h"
#include "Environment/WeatherDomeComponent.h"
#include "Environment/WeatherOverlayComponent.h"

namespace toy {

class WeatherManager
{
public:
    WeatherManager() = default;

    //==================================================================
    // Update
    //==================================================================
    void Update(float deltaTime);

    // SkyDome（空・光源）側との連携
    void SetWeatherDome(class toy::WeatherDomeComponent* dome)
    {
        mWeatherDome = dome;
    }

    // Overlay（雨粒/雪/雷など）側との連携
    void SetWeatherOverlay(class toy::WeatherOverlayComponent* overlay)
    {
        mWeatherOverlay = overlay;
    }

    //==================================================================
    // ChangeWeather
    //==================================================================
    // 天候を変更する。
    // 反映先:
    //   - SkyDome   : 空色、雲、太陽/月の描画方針
    //   - Overlay   : 雨量、霧量、雪量、フレア許可条件
    //==================================================================
    void ChangeWeather(WeatherType weather);

    WeatherType GetWeatherType() const { return mWeather; }

private:
    // 演出値を現在天候から計算して Overlay に適用
    void ApplyWeatherParams();

private:
    class WeatherOverlayComponent* mWeatherOverlay { nullptr };
    class WeatherDomeComponent*    mWeatherDome    { nullptr };

    WeatherType mWeather { WeatherType::SIMPLE };
};

} // namespace toy
