#pragma once

#include "Graphics/VisualComponent.h"
#include "Render/RenderQueue.h"
#include "Utils/MathUtil.h"
#include <memory>

namespace toy {

class VertexArray;

class WeatherOverlayComponent : public VisualComponent
{
public:
    WeatherOverlayComponent(Actor* a,
                            int drawOrder = 1000,
                            VisualLayer layer = VisualLayer::OverlayScreen);

    void GatherRenderItems(RenderQueue& outQueue) override;

    //==========================================================
    // Parameter setters
    //  - WeatherType は知らず、描画パラメータだけ受け取る
    //==========================================================
    void SetRainAmount(float v)       { mRainAmount = v; }
    void SetFogAmount(float v)        { mFogAmount = v; }
    void SetSnowAmout(float v)        { mSnowAmount = v; }   // 既存名維持
    void SetFlareIntensity(float v)   { mFlareIntensity = v; }

    void SetSunDir(const Vector3& v)  { mSunDir = v; }
    void SetMoonDir(const Vector3& v) { mMoonDir = v; }

    void SetFlareColor(const Vector3& v) { mFlareColor = v; }

    float GetRainAmount() const       { return mRainAmount; }
    float GetFogAmount() const        { return mFogAmount; }
    float GetSnowAmount() const       { return mSnowAmount; }
    float GetFlareIntensity() const   { return mFlareIntensity; }

private:
    std::shared_ptr<VertexArray> mVertexArray;
    std::string mPipelineName;

    // 描画パラメータ
    float mRainAmount     { 0.0f };
    float mFogAmount      { 0.0f };
    float mSnowAmount     { 0.0f };
    float mFlareIntensity { 0.0f };

    Vector3 mSunDir       { Vector3::Zero };
    Vector3 mMoonDir      { Vector3::Zero };

    Vector3 mFlareColor   { 1.0f, 0.9f, 0.7f };
};

} // namespace toy
