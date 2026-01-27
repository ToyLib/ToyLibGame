#include "Environment/WeatherManager.h"
#include "Utils/MathUtil.h"

namespace toy {


void WeatherManager::Update(float deltaTime)
{
    // ------------------------------------------------------------
    // Dome（空・太陽・光源）と Overlay（雨・雪・霧）の
    // 両方が設定されている場合のみ天候を反映する。
    //
    // ・SkyDome … 空や色、フォグ色など
    // ・Overlay … パーティクル（雨・雪）、フォグ量など
    // ------------------------------------------------------------
    if (!mWeatherDome || !mWeatherOverlay)
    {
        return;
    }
    
    // スカイドーム側にも天気を反映
    mWeatherDome->SetWeatherType(mWeather);

    // オーバーレイ側にも反映（雨・雪・霧の強さなど）
    ChangeWeather(mWeather);
    
    // 太陽/月の方向取得
    Vector3 sunDir = mWeatherDome->GetSunDir();
    Vector3 moonDir = mWeatherDome->GetMoonDir();
    // オーバーレイ側に反映
    mWeatherOverlay->SetSunDir(sunDir);
    mWeatherOverlay->SetMoonDir(moonDir);
}


void WeatherManager::ChangeWeather(const WeatherType weather)
{
    // ------------------------------------------------------------
    // WeatherType に応じた強度パラメータを決定
    // ・rainAmount … 雨粒の発生量
    // ・fogAmount  … フォグの濃度
    // ・snowAmount … 雪粒の発生量
    //
    // ※ここは数値のチューニングポイント
    // ------------------------------------------------------------
    float rainAmount = 0.0f;
    float fogAmount  = 0.0f;
    float snowAmount = 0.0f;

    switch (weather)
    {
        case WeatherType::CLEAR:
            rainAmount = 0.0f;
            fogAmount  = 0.0f;
            break;

        case WeatherType::CLOUDY:
            rainAmount = 0.0f;
            fogAmount  = 0.1f;
            break;

        case WeatherType::RAIN:
            rainAmount = 0.4f;
            fogAmount  = 0.3f;
            break;

        case WeatherType::STORM:
            rainAmount = 0.7f;
            fogAmount  = 0.4f;
            break;

        case WeatherType::SNOW:
            rainAmount = 0.0f;
            fogAmount  = 0.7f;
            snowAmount = 0.8f;
            break;

        default:
            break;
    }

    // 現在の天気を更新
    mWeather = weather;

    // ------------------------------------------------------------
    // SkyDome へ天気を伝える（空の色・光源・フォグ色）
    // ------------------------------------------------------------
    if (mWeatherDome)
        mWeatherDome->SetWeatherType(mWeather);

    // ------------------------------------------------------------
    // Overlay へ具体的な演出値を伝える（雨量、霧量、雪量）
    // ------------------------------------------------------------
    if (mWeatherOverlay)
    {
        mWeatherOverlay->SetRainAmount(rainAmount);
        mWeatherOverlay->SetFogAmount(fogAmount);
        mWeatherOverlay->SetSnowAmout(snowAmount);
    }
}

} // namespace toy
