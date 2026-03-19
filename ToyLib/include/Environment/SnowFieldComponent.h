#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <vector>
#include <string>

namespace toy {

class Texture;
class RenderQueue;

struct SnowParticle
{
    Vector3 position { Vector3::Zero };
    Vector3 velocity { Vector3::Zero };
    float   scale    { 1.0f };
    float   alpha    { 1.0f };
};

class SnowFieldComponent : public VisualComponent
{
public:
    SnowFieldComponent(Actor* owner,
                       int drawOrder = 200,
                       VisualLayer layer = VisualLayer::Object3D);

    void Update(float deltaTime) override;
    void GatherRenderItems(RenderQueue& out) override;

    void SetEnabled(bool enabled)      { mEnabled = enabled; }
    void SetIntensity(float v)         { mIntensity = Math::Clamp(v, 0.0f, 1.0f); }
    void SetWind(const Vector3& wind)  { mWind = wind; }
    void SetFallSpeed(float v)         { mFallSpeed = v; }
    void SetAreaSize(const Vector3& v) { mAreaSize = v; }

    void SetBaseScale(float s)         { mBaseScale = s; }
    void SetYawOnly(bool v)            { mYawOnly = v; }

private:
    void InitializeParticles();
    void RespawnParticle(SnowParticle& p, const Vector3& center, bool randomY);
    Vector3 GetSpawnCenter() const;

private:
    std::vector<SnowParticle> mParticles;

    bool  mEnabled   { true };
    float mIntensity { 1.0f };

    int     mMaxParticles        { 256 };
    float   mFallSpeed           { 3.0f };
    Vector3 mWind                { 0.3f, 0.0f, 0.1f };
    Vector3 mAreaSize            { 20.0f, 12.0f, 20.0f };
    float   mRespawnBottomMargin { 2.0f };

    float mBaseScale { 0.02f };
    bool  mYawOnly   { true };
};

} // namespace toy
