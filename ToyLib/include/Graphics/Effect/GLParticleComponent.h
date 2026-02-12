#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>
#include <string>
#include <cstdint>

namespace toy {


class GLParticleComponent : public VisualComponent
{
public:
    enum class ParticleMode
    {
        Spark = 0,
        Water = 1,
        Smoke = 2
    };

    //==============================================================
    // Desc（★元仕様を厳守）
    //==============================================================
    struct Desc
    {
        ParticleMode mode { ParticleMode::Spark };
        uint32_t maxParticles { 64 };

        float componentLife   { 0.0f }; // ★復活・維持
        float particleLife    { 0.6f };
        float size            { 1.0f };

        float spawnRatePerSec { 60.0f };
        float spawnRampSec    { 0.0f };

        float spread          { 2.0f };
        float gravity         { 0.0f };
        float lift            { 0.0f };

        bool  additiveBlend   { true };
        bool  warmStart       { true };

        Vector3 emitterOffset { Vector3::Zero };
    };

public:
    GLParticleComponent(class Actor* owner, int drawOrder = 20);
    ~GLParticleComponent();

    void Update(float deltaTime) override;
    void SetTexture(std::shared_ptr<class Texture> tex) override;

    void Init(const Desc& desc);
    bool InitFromFile(const std::string& filePath);

    void Start();
    void Stop();
    void Reset();

    //==============================================================
    // RenderQueue 用
    //==============================================================
    void GatherRenderItems(class RenderQueue& outQueue) override;

private:
    static ParticleMode ParseModeString(const std::string& s);

    void ApplyModePresetIfNeeded();

    void InitIfNeeded();
    void ReleaseGL();

    void InitQuadGeometry();
    void InitUpdateVAO();
    void InitRenderVAO();
    void InitParticleBuffers(bool warmStart);
    void UpdateParticlesGPU(float deltaTime);

    unsigned int CurrentSrcVBO() const;
    unsigned int CurrentDstVBO() const;

    void BindUpdateAttributes(unsigned int srcVBO);
    void BindInstanceAttributes(unsigned int srcVBO);

private:
    std::shared_ptr<Texture> mTexture;
    class GLShader*  mUpdateShader;
    std::string mUpdatePipelineName;

    Desc  mDesc {};
    bool  mInitialized { false };
    bool  mPingPong    { false };
    bool  mRunning     { false };

    float mComponentLifeAcc { 0.0f };
    float mTimeAcc          { 0.0f };

    unsigned int mQuadVBO {};
    unsigned int mQuadIBO {};

    unsigned int mParticleVBO_A {};
    unsigned int mParticleVBO_B {};

    unsigned int mUpdateVAO {};
    unsigned int mRenderVAO {};

    bool mPendingHardReset { true };
    int  mSkipDrawFrames   { 2 };
};

} // namespace toy
