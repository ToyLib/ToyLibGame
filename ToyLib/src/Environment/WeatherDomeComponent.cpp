// Environment/WeatherDomeComponent.cpp
#include "Environment/WeatherDomeComponent.h"
#include "Environment/SkyDomeMeshGenerator.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/GL/GLRenderer.h"
#include "Render/RenderItem.h"
#include "Render/RenderQueue.h"
#include "Engine/Runtime/TimeOfDaySystem.h"
#include "Render/LightingManager.h"
#include "Asset/Geometry/VertexArray.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

namespace toy {

//==============================================================
// ctor
//==============================================================
WeatherDomeComponent::WeatherDomeComponent(Actor* owner, int drawOrder)
    : SkyDomeComponent(owner, drawOrder, VisualLayer::Sky)
{
    mMoonDir.Normalize();

    mSkyVAO = SkyDomeMeshGenerator::CreateSkyDomeVAO(32, 16, 1.0f);

    mPipelineName = "SkyDome";
}

//==============================================================
// GatherRenderItems
//==============================================================
void WeatherDomeComponent::GatherRenderItems(RenderQueue& outQueue)
{
    if (!mIsVisible)
    {
        return;
    }
    if (!mSkyVAO)
    {
        return;
    }

    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    const Matrix4 invView = renderer->GetInvViewMatrix();
    const Vector3 camPos  = invView.GetTranslation();

    const Matrix4 world =
        Matrix4::CreateScale(200.0f) *
        Matrix4::CreateTranslation(camPos);

    const Matrix4 view     = renderer->GetViewMatrix();
    const Matrix4 proj     = renderer->GetProjectionMatrix();
    const Matrix4 viewProj = view * proj;

    SkyDomePayload sky{};

    const float sec = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    sky.skyTime = std::fmod(sec, 60.0f) / 60.0f;

    sky.skyTimeOfDay     = std::fmod(mTime, 1.0f);
    sky.skyWeatherType   = static_cast<int>(mWeatherType);
    sky.skySunDir        = mSunDir;
    sky.skyMoonDir       = mMoonDir;
    sky.skyRawSkyColor   = mRawSkyColor;
    sky.skyRawCloudColor = mRawCloudColor;

    sky.world = world;

    sky.useMVP = true;
    sky.mvp    = world * view * proj;

    const uint32_t payloadIndex = outQueue.PushSkyDomePayload(sky);

    RenderItem it{};
    it.pass      = RenderPass::World;
    it.layer     = VisualLayer::Sky;
    it.drawOrder = GetDrawOrder();

    it.type     = RenderItemType::SkyDome;
    it.dispatch = GetDispatch(it.type);

    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = BlendMode::Opaque;

    it.cull      = CullMode::None;
    it.frontFace = FrontFace::CCW;

    it.pipeline     = renderer->GetPipelineHandle(mPipelineName);
    it.geometry.ptr = mSkyVAO.get();

    it.world    = world;
    it.viewProj = viewProj;

    it.topology   = PrimitiveTopology::Triangles;
    it.indexCount = mSkyVAO->GetNumIndices();

    it.payloadIndex = payloadIndex;

    outQueue.Push(it);
}

//======================================
// SmoothStep
//======================================
float WeatherDomeComponent::SmoothStep(float edge0, float edge1, float x)
{
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

//======================================
// Update
//======================================
void WeatherDomeComponent::Update(float deltaTime)
{
    (void)deltaTime;

    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }

    auto* timeSystem = app->GetTimeOfDaySystem();
    if (!timeSystem)
    {
        return;
    }

    float hour = timeSystem->GetHourFloat();
    float t    = hour / 24.0f;
    mTime      = t;

    ApplyTime();
}

//======================================
// GetSkyColor
//  - SIMPLE/CLEAR は時間帯ベースの空色をそのまま使う
//  - CLOUDY/RAIN/STORM/SNOW は曇天寄りに補正
//======================================
Vector3 WeatherDomeComponent::GetSkyColor(float time)
{
    Vector3 night(0.02f, 0.03f, 0.08f);
    Vector3 day  (0.55f, 0.75f, 1.00f);
    Vector3 dusk (0.95f, 0.55f, 0.35f);

    time = fmodf(time, 1.0f);
    if (time < 0.0f)
    {
        time += 1.0f;
    }

    const float sunriseHour  = 6.0f;
    const float sunsetHour   = 18.0f;
    const float dawnSpanHour = 1.0f;
    const float duskSpanHour = 1.0f;

    const float sunriseT   = sunriseHour   / 24.0f;
    const float sunsetT    = sunsetHour    / 24.0f;
    const float dawnSpanT  = dawnSpanHour  / 24.0f;
    const float duskSpanT  = duskSpanHour  / 24.0f;

    const float tNightEnd1   = sunriseT - dawnSpanT;
    const float tDawnMid     = sunriseT;
    const float tDawnEnd     = sunriseT + dawnSpanT;
    const float tDuskStart   = sunsetT - duskSpanT;
    const float tDuskMid     = sunsetT;
    const float tNightStart2 = sunsetT + duskSpanT;

    auto smooth01 = [](float x)
    {
        x = Math::Clamp(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    };

    Vector3 col;

    if (time < tNightEnd1 || time >= tNightStart2)
    {
        col = night;
    }
    else if (time < tDawnMid)
    {
        float u = (time - tNightEnd1) / (tDawnMid - tNightEnd1);
        u = smooth01(u);
        col = Vector3::Lerp(night, dusk, u);
    }
    else if (time < tDawnEnd)
    {
        float u = (time - tDawnMid) / (tDawnEnd - tDawnMid);
        u = smooth01(u);
        col = Vector3::Lerp(dusk, day, u);
    }
    else if (time < tDuskStart)
    {
        float dayMid = (tDawnEnd + tDuskStart) * 0.5f;
        float w = 1.0f - fabsf(time - dayMid) / (tDuskStart - tDawnEnd);
        w = Math::Clamp(w, 0.0f, 1.0f);
        float bright = 0.9f + 0.1f * smooth01(w);
        col = day * bright;
    }
    else if (time < tDuskMid)
    {
        float u = (time - tDuskStart) / (tDuskMid - tDuskStart);
        u = smooth01(u);
        col = Vector3::Lerp(day, dusk, u);
    }
    else
    {
        float u = (time - tDuskMid) / (tNightStart2 - tDuskMid);
        u = smooth01(u);
        col = Vector3::Lerp(dusk, night, u);
    }

    // SIMPLE/CLEAR はベース空色をそのまま使う
    // CLOUDY/RAIN/STORM/SNOW は曇天寄りに補正
    const bool overcastWeather =
        (mWeatherType == WeatherType::CLOUDY) ||
        (mWeatherType == WeatherType::RAIN)   ||
        (mWeatherType == WeatherType::STORM)  ||
        (mWeatherType == WeatherType::SNOW);

    if (overcastWeather)
    {
        Vector3 overcast(0.65f, 0.68f, 0.72f);

        float overcastStrength = 0.75f;
        col = Vector3::Lerp(col, overcast, overcastStrength);

        float g = (col.x + col.y + col.z) / 3.0f;
        Vector3 gray(g, g, g);

        float desat = 0.6f;
        col = Vector3::Lerp(col, gray, desat);
    }

    return col;
}

//======================================
// GetCloudColor
//  - 雲色そのものは時間帯ベースで返す
//  - SIMPLE でも値は返してよい（shader 側で cloudAlpha=0 にするため）
//======================================
Vector3 WeatherDomeComponent::GetCloudColor(float time)
{
    Vector3 dayColor   (0.95f, 0.98f, 1.00f);
    Vector3 duskColor  (1.00f, 0.75f, 0.55f);
    Vector3 nightColor (0.16f, 0.18f, 0.26f);

    time = fmodf(time, 1.0f);
    if (time < 0.0f)
    {
        time += 1.0f;
    }

    const float sunriseHour  = 5.0f;
    const float sunsetHour   = 18.0f;
    const float dawnSpanHour = 1.0f;
    const float duskSpanHour = 1.0f;

    const float sunriseT   = sunriseHour   / 24.0f;
    const float sunsetT    = sunsetHour    / 24.0f;
    const float dawnSpanT  = dawnSpanHour  / 24.0f;
    const float duskSpanT  = duskSpanHour  / 24.0f;

    const float tNightEnd1   = sunriseT - dawnSpanT;
    const float tDawnMid     = sunriseT;
    const float tDawnEnd     = sunriseT + dawnSpanT;
    const float tDuskStart   = sunsetT - duskSpanT;
    const float tDuskMid     = sunsetT;
    const float tNightStart2 = sunsetT + duskSpanT;

    auto smooth01 = [](float x)
    {
        x = Math::Clamp(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    };

    Vector3 col;

    if (time < tNightEnd1 || time >= tNightStart2)
    {
        col = nightColor;
    }
    else if (time < tDawnMid)
    {
        float u = (time - tNightEnd1) / (tDawnMid - tNightEnd1);
        u = smooth01(u);
        col = Vector3::Lerp(nightColor, duskColor, u);
    }
    else if (time < tDawnEnd)
    {
        float u = (time - tDawnMid) / (tDawnEnd - tDawnMid);
        u = smooth01(u);
        col = Vector3::Lerp(duskColor, dayColor, u);
    }
    else if (time < tDuskStart)
    {
        col = dayColor;
    }
    else if (time < tDuskMid)
    {
        float u = (time - tDuskStart) / (tDuskMid - tDuskStart);
        u = smooth01(u);
        col = Vector3::Lerp(dayColor, duskColor, u);
    }
    else
    {
        float u = (time - tDuskMid) / (tNightStart2 - tDuskMid);
        u = smooth01(u);
        col = Vector3::Lerp(duskColor, nightColor, u);
    }

    float nightFactor = 0.0f;
    if (time < tNightEnd1)
    {
        nightFactor = 1.0f - (time / tNightEnd1);
    }
    else if (time >= tNightStart2)
    {
        nightFactor = (time - tNightStart2) / (1.0f - tNightStart2);
    }
    nightFactor = Math::Clamp(nightFactor, 0.0f, 1.0f);

    bool badWeather =
        (mWeatherType == WeatherType::CLOUDY) ||
        (mWeatherType == WeatherType::RAIN)   ||
        (mWeatherType == WeatherType::STORM)  ||
        (mWeatherType == WeatherType::SNOW);

    if (badWeather && nightFactor > 0.0f)
    {
        Vector3 darkTarget(0.08f, 0.09f, 0.12f);

        float darkStrength = 0.4f;
        switch (mWeatherType)
        {
            case WeatherType::CLOUDY: darkStrength = 0.6f; break;
            case WeatherType::RAIN:   darkStrength = 0.8f; break;
            case WeatherType::STORM:  darkStrength = 0.9f; break;
            case WeatherType::SNOW:   darkStrength = 0.5f; break;
            case WeatherType::SIMPLE:
            case WeatherType::CLEAR:
            default:
                break;
        }

        float w = nightFactor * darkStrength;

        col = Vector3::Lerp(col, darkTarget, w);

        float g = (col.x + col.y + col.z) / 3.0f;
        Vector3 gray(g, g, g);
        float desat = 0.4f * nightFactor;
        col = Vector3::Lerp(col, gray, desat);
    }

    return col;
}

//======================================
// ApplyTime
//======================================
void WeatherDomeComponent::ApplyTime()
{
    float timeOfDay = fmod(mTime, 1.0f);
    if (timeOfDay < 0.0f)
    {
        timeOfDay += 1.0f;
    }

    //==============================================================
    // 太陽・月の方向
    //==============================================================
    float sunAngle  = Math::TwoPi * (timeOfDay - 0.25f);
    float moonAngle = sunAngle + Math::Pi;

    const float sunElevationScale  = 0.6f;
    const float sunVerticalOffset  = -0.1f;
    const float sunSideOffset      = 0.4f;

    const float moonElevationScale = 0.5f;
    const float moonVerticalOffset = 0.05f;
    const float moonSideOffset     = 0.4f;

    mSunDir = Vector3(
        -cosf(sunAngle),
        -sinf(sunAngle) * sunElevationScale + sunVerticalOffset,
        sunSideOffset * sinf(sunAngle)
    );
    mSunDir.Normalize();

    mMoonDir = Vector3(
        -cosf(moonAngle),
        -sinf(moonAngle) * moonElevationScale + moonVerticalOffset,
        moonSideOffset * sinf(moonAngle)
    );
    mMoonDir.Normalize();

    //==============================================================
    // 空色・雲色
    //==============================================================
    mRawSkyColor   = GetSkyColor(timeOfDay);
    mRawCloudColor = GetCloudColor(timeOfDay);

    //==============================================================
    // 天気による全体減衰
    //  - SIMPLE は演出なしだが、明るさは CLEAR と同等でよい
    //==============================================================
    float weatherDim = 1.0f;
    switch (mWeatherType)
    {
        case WeatherType::SIMPLE:  weatherDim = 1.0f; break;
        case WeatherType::CLEAR:   weatherDim = 1.0f; break;
        case WeatherType::CLOUDY:  weatherDim = 0.7f; break;
        case WeatherType::RAIN:    weatherDim = 0.5f; break;
        case WeatherType::STORM:   weatherDim = 0.3f; break;
        case WeatherType::SNOW:    weatherDim = 0.6f; break;
        default:                   weatherDim = 1.0f; break;
    }

    //==============================================================
    // 昼夜の強さ
    //==============================================================
    float dayStrength =
        SmoothStep(0.15f, 0.25f, timeOfDay) *
        (1.0f - SmoothStep(0.75f, 0.85f, timeOfDay));
    float nightStrength = 1.0f - dayStrength;

    //==============================================================
    // ライティング更新
    //  - 昼は太陽、夜は月方向
    //  - SIMPLE でもここは普通に有効
    //==============================================================
    if (mLightingManager)
    {
        mLightingManager->SetSunIntensity(dayStrength);

        Vector3 lightDir;
        if (dayStrength > 0.01f)
        {
            lightDir = Vector3(-mSunDir.x, -mSunDir.y, -mSunDir.z);
        }
        else
        {
            lightDir = Vector3(-mMoonDir.x, -mMoonDir.y, -mMoonDir.z);
        }
        mLightingManager->SetLightDirection(lightDir, Vector3::Zero);

        Vector3 sunColor  = Vector3(1.0f, 0.95f, 0.8f);
        Vector3 moonColor = Vector3(0.25f, 0.35f, 0.5f);
        Vector3 finalLightColor =
            (sunColor * dayStrength + moonColor * nightStrength) * weatherDim;
        mLightingManager->SetLightDiffuseColor(finalLightColor);

        Vector3 dayAmbient   = Vector3(0.7f, 0.7f, 0.7f);
        Vector3 nightAmbient = Vector3(0.25f, 0.2f, 0.3f);
        Vector3 finalAmbient =
            (dayAmbient * dayStrength + nightAmbient * nightStrength) * weatherDim;
        mLightingManager->SetAmbientColor(finalAmbient);
    }

    ComputeFogFromSky(timeOfDay);
}

//======================================
// ComputeFogFromSky
//======================================
void WeatherDomeComponent::ComputeFogFromSky(float timeOfDay)
{
    Vector3 overcastBase(0.4f, 0.4f, 0.5f);

    // SIMPLE/CLEAR は sky color ベースをそのまま活かす
    // CLOUDY は少しだけ残す
    // RAIN/STORM/SNOW は overcastBase 寄り
    float weatherFade = 1.0f;
    switch (mWeatherType)
    {
        case WeatherType::SIMPLE:
        case WeatherType::CLEAR:
            weatherFade = 1.0f;
            break;

        case WeatherType::CLOUDY:
            weatherFade = 0.25f;
            break;

        case WeatherType::RAIN:
        case WeatherType::STORM:
        case WeatherType::SNOW:
            weatherFade = 0.0f;
            break;

        default:
            weatherFade = 1.0f;
            break;
    }

    Vector3 baseSky = Vector3::Lerp(overcastBase, mRawSkyColor, weatherFade);

    float t = 0.1f;
    Vector3 skyHorizon = Vector3::Lerp(baseSky * 0.6f, baseSky, t);

    Vector3 cloudColor = mRawCloudColor;
    float cloudMix = 0.0f;

    switch (mWeatherType)
    {
        case WeatherType::SIMPLE:
            cloudMix    = 0.10f;
            mFogDensity = 0.0015f;
            break;

        case WeatherType::CLEAR:
            cloudMix    = 0.20f;
            mFogDensity = 0.0020f;
            break;

        case WeatherType::CLOUDY:
            cloudMix    = 0.50f;
            mFogDensity = 0.0040f;
            skyHorizon = Vector3::Lerp(skyHorizon, Vector3(0.3f, 0.3f, 0.32f), 0.4f);
            break;

        case WeatherType::RAIN:
            cloudMix    = 0.70f;
            mFogDensity = 0.0080f;
            skyHorizon  = Vector3(0.20f, 0.20f, 0.21f);
            cloudColor  = skyHorizon;
            break;

        case WeatherType::STORM:
            cloudMix    = 0.90f;
            mFogDensity = 0.0150f;
            skyHorizon  = Vector3(0.12f, 0.12f, 0.13f);
            cloudColor  = skyHorizon;
            break;

        case WeatherType::SNOW:
            cloudMix    = 0.70f;
            mFogDensity = 0.0100f;
            skyHorizon  = Vector3(0.80f, 0.80f, 0.82f);
            cloudColor  = skyHorizon;
            break;

        default:
            cloudMix    = 0.20f;
            mFogDensity = 0.0020f;
            break;
    }

    Vector3 fog = Vector3::Lerp(skyHorizon, cloudColor, cloudMix);

    float nightFactor = 0.0f;
    if (timeOfDay < 0.25f)
    {
        nightFactor = (0.25f - timeOfDay) / 0.25f;
    }
    else if (timeOfDay > 0.75f)
    {
        nightFactor = (timeOfDay - 0.75f) / 0.25f;
    }

    fog *= (1.0f - 0.6f * nightFactor);

    mFogColor = fog;

    if (mLightingManager)
    {
        mLightingManager->SetFogColor(mFogColor);
    }
}

} // namespace toy
