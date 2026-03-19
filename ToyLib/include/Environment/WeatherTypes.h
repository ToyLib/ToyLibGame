// Environment/WeatherTypes.h
#pragma once

namespace toy {

//======================================================================
// 天候タイプ
//======================================================================
enum class WeatherType
{
    SIMPLE = 0,  // 最低限の空。雲/太陽/月なし。夜は星のみ
    CLEAR,       // 快晴
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
    WeatherType type      { WeatherType::SIMPLE };
    float rainAmount      { 0.0f };
    float fogAmount       { 0.0f };
    float snowAmount      { 0.0f };
};

} // namespace toy
