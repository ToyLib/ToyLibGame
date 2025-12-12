// Graphics/Effect/GPUParticleComponent.h
#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>
#include <string>

namespace toy {

class Texture;
class Shader;

class GPUParticleComponent : public VisualComponent
{
public:
    enum class ParticleMode
    {
        Spark = 0,
        Water = 1,
        Smoke = 2
    };

    struct Desc
    {
        ParticleMode mode = ParticleMode::Spark;

        uint32_t maxParticles = 64;

        float componentLife   = 0.0f;  // 0 = infinite
        float particleLife    = 0.6f;  // seconds
        float size            = 1.0f;

        float spawnRatePerSec = 60.0f; // how many respawns per second (approx)
        float spawnRampSec    = 0.0f;  // 0 = no ramp

        float spread          = 2.0f;

        float gravity         = 0.0f;
        float lift            = 0.0f;

        bool  additiveBlend   = true;
        bool  warmStart       = true;

        Vector3 emitterOffset = Vector3::Zero; // Actor local offset
    };

public:
    GPUParticleComponent(class Actor* owner, int drawOrder = 20);
    ~GPUParticleComponent();

    void Update(float deltaTime) override;
    void Draw() override;

    void SetTexture(std::shared_ptr<Texture> tex) override;

    // New API
    void Init(const Desc& desc);
    bool InitFromFile(const std::string& filePath); // ★追加
    void Start();
    void Stop();
    void Reset();

private:
    static ParticleMode ParseModeString(const std::string& s);

    void ApplyModePresetIfNeeded();
    void InitIfNeeded();
    void ReleaseGL();

    void InitQuadGeometry();
    void InitUpdateVAO();
    void InitRenderVAO();
    void InitParticleBuffers(bool warmStart);

    void UpdateParticlesGPU(float dt);

    unsigned int CurrentSrcVBO() const;
    unsigned int CurrentDstVBO() const;

    void BindUpdateAttributes(unsigned int srcVBO);
    void BindInstanceAttributes(unsigned int srcVBO);

private:
    std::shared_ptr<Texture> mTexture;
    std::shared_ptr<Shader>  mUpdateShader;
    std::shared_ptr<Shader>  mRenderShader;

    Desc  mDesc{};
    bool  mInitialized = false;
    bool  mPingPong    = false;
    bool  mRunning     = false;

    float mComponentLifeAcc = 0.0f;
    float mTimeAcc          = 0.0f;

    // GL
    unsigned int mQuadVBO = 0;
    unsigned int mQuadIBO = 0;
    unsigned int mParticleVBO_A = 0;
    unsigned int mParticleVBO_B = 0;
    unsigned int mUpdateVAO = 0;
    unsigned int mRenderVAO = 0;
};

} // namespace toy
