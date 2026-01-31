#pragma once

#include "Environment/SkyDomeComponent.h"
#include "Environment/WeatherType.h"
#include "Utils/MathUtil.h"

namespace toy {

//==============================================
// WeatherDomeComponent
//  - “空 + 天候 + 昼夜 + ライティング更新”
//  - Update: 元の挙動を尊重（TimeOfDaySystem → mTime → ApplyTime）
//  - 描画: 新パスへ移行（GatherRenderItems）
//==============================================
class WeatherDomeComponent : public SkyDomeComponent
{
public:
    WeatherDomeComponent(class Actor* owner, int drawOrder = 0);

    // 旧パス互換（使わない前提なら空でOK）
    void Draw() override {}

    // 元挙動：TimeOfDaySystem から時刻を取り ApplyTime()
    void Update(float deltaTime) override;

    // 新パス：RenderItem を積む
    void GatherRenderItems(class RenderQueue& outQueue) override;

    // 元のAPI（WeatherManager が使う）
    WeatherType GetWeatherType() const { return mWeatherType; }
    void SetWeatherType(WeatherType weather) { mWeatherType = weather; } // ← “受け取って保持”だけ（元通り）

    const Vector3& GetSunDir()  const { return mSunDir; }
    const Vector3& GetMoonDir() const { return mMoonDir; }

private:
    //==============================================================
    // 元の内部状態（そのまま）
    //==============================================================
    float      mTime { 0.5f };                    // 0..1
    Vector3    mSunDir  { Vector3::UnitY };
    Vector3    mMoonDir { Vector3::NegUnitY };
    WeatherType mWeatherType { WeatherType::CLEAR };

    Vector3 mRawSkyColor   { Vector3::Zero };
    Vector3 mRawCloudColor { Vector3::Zero };
    Vector3 mFogColor      { Vector3::Zero };
    float   mFogDensity    { 0.0f };

private:
    //==============================================================
    // 元の計算関数（内容は cpp にそのまま移植）
    //==============================================================
    float SmoothStep(float edge0, float edge1, float x);

    void   ApplyTime();
    void   ComputeFogFromSky(float timeOfDay);

    Vector3 GetCloudColor(float time);
    Vector3 GetSkyColor(float time);
};

} // namespace toy
