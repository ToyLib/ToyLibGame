#include "Environment/WeatherManager.h"
#include "Utils/MathUtil.h"

namespace toy {

void WeatherManager::Update(float deltaTime)
{
    (void)deltaTime;

    // SkyDome 側に現在天候を反映
    if (mWeatherDome)
    {
        mWeatherDome->SetWeatherType(mWeather);
    }

    // Dome から天体方向を Overlay へ渡す
    if (mWeatherOverlay && mWeatherDome)
    {
        mWeatherOverlay->SetSunDir(mWeatherDome->GetSunDir());
        mWeatherOverlay->SetMoonDir(mWeatherDome->GetMoonDir());
    }

    ApplyWeatherParams();
}

void WeatherManager::ChangeWeather(WeatherType weather)
{
    mWeather = weather;

    if (mWeatherDome)
    {
        mWeatherDome->SetWeatherType(mWeather);
    }

    if (mWeatherOverlay && mWeatherDome)
    {
        mWeatherOverlay->SetSunDir(mWeatherDome->GetSunDir());
        mWeatherOverlay->SetMoonDir(mWeatherDome->GetMoonDir());
    }

    ApplyWeatherParams();
}

void WeatherManager::ApplyWeatherParams()
{
    float rainAmount     = 0.0f;
    float fogAmount      = 0.0f;
    float snowAmount     = 0.0f;
    float flareIntensity = 0.0f;

    switch (mWeather)
    {
        case WeatherType::SIMPLE:
            rainAmount     = 0.0f;
            fogAmount      = 0.0f;
            snowAmount     = 0.0f;
            flareIntensity = 0.0f;
            break;

        case WeatherType::CLEAR:
            rainAmount     = 0.0f;
            fogAmount      = 0.0f;
            snowAmount     = 0.0f;
            flareIntensity = 1.0f;
            break;

        case WeatherType::CLOUDY:
            rainAmount     = 0.0f;
            fogAmount      = 0.1f;
            snowAmount     = 0.0f;
            flareIntensity = 0.0f;
            break;

        case WeatherType::RAIN:
            rainAmount     = 0.4f;
            fogAmount      = 0.3f;
            snowAmount     = 0.0f;
            flareIntensity = 0.0f;
            break;

        case WeatherType::STORM:
            rainAmount     = 0.7f;
            fogAmount      = 0.4f;
            snowAmount     = 0.0f;
            flareIntensity = 0.0f;
            break;

        case WeatherType::SNOW:
            rainAmount     = 0.0f;
            fogAmount      = 0.6f;
            snowAmount     = 0.8f;
            flareIntensity = 0.0f;
            break;

        default:
            break;
    }

    if (mWeatherOverlay)
    {
        mWeatherOverlay->SetRainAmount(rainAmount);
        mWeatherOverlay->SetFogAmount(fogAmount);
        mWeatherOverlay->SetSnowAmout(snowAmount);
        mWeatherOverlay->SetFlareIntensity(flareIntensity);
    }
}

} // namespace toy
