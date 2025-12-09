#pragma once
#include "Environment/SkyDomeComponent.h"
#include "Environment/WeatherType.h"

namespace toy {

//==============================================
// 時間帯・天候に応じてスカイドームを描画する派生クラス
//   └ SkyDomeComponent は基底の "空 VAO / Shader" 管理だけ担当
//==============================================
class WeatherDomeComponent : public SkyDomeComponent
{
public:
    WeatherDomeComponent(class Actor* a);
    
    // スカイドーム描画
    void Draw() override;

    // 時間帯進行・天候補間・色の更新
    void Update(float deltaTime) override;
    
    // 時間帯 (0.0〜1.0 … 夜→昼→夜)
    void SetTime(float t);

    // 太陽方向（ライト計算のベース）
    void SetSunDirection(const Vector3& dir);
    
    // 天気
    WeatherType GetWeatherType() const { return mWeatherType; }
    void SetWeatherType(WeatherType weather) { mWeatherType = weather; }
    
private:
    //==============================================
    // 基本状態
    //==============================================
    float    mTime;       // 時間帯 (0〜1)
    Vector3  mSunDir;     // 太陽方向（ワールド座標）
    Vector3  mMoonDir;    // 月方向
    WeatherType mWeatherType; // 現在の天候
    
    //==============================================
    // シェーダーへ渡す元データ（既存 SkyDome 表現）
    //==============================================
    Vector3 mRawSkyColor;     // 空の基本カラー
    Vector3 mRawCloudColor;   // 雲の基本カラー
    Vector3 mFogColor;        // 霧色（地平線付近）
    float   mFogDensity;      // 霧の濃さ
    
    //==============================================
    // 時間帯・天候による補正関数
    //==============================================
    
    // 基本の SmoothStep（補間）
    float SmoothStep(float edge0, float edge1, float x);

    // 時間帯に応じて raw 色をセット
    void ApplyTime();
    
    // 朝夕の色味からフォグ色を求める
    void ComputeFogFromSky(float timeOfDay);

    // 時間帯による雲色
    Vector3 GetCloudColor(float time);

    // 時間帯による空色
    Vector3 GetSkyColor(float time);
};

} // namespace toy
