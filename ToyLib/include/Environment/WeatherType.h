// Environment/WeatherTypes.h
#pragma once

namespace toy {

//======================================================================
// 天候タイプ
//======================================================================
enum class WeatherType
{
    CLEAR = 0,   // 快晴
    CLOUDY,      // 曇り
    RAIN,        // 雨
    STORM,       // 嵐（雷雨などを含む）
    SNOW         // 雪
};

//======================================================================
// 天候の詳細パラメータ
//======================================================================
struct WeatherState
{
    WeatherType type      = WeatherType::CLEAR;
    float rainAmount      = 0.0f;
    float fogAmount       = 0.0f;
    float snowAmount      = 0.0f;
};

} // namespace toy
