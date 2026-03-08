#pragma once

#include "Graphics/Effect/IParticleBackend.h"

#include <memory>
#include <string>

namespace toy
{

class GLShader;
class VisualComponent;
class RenderQueue;

class GLParticleBackend : public IParticleBackend
{
public:
    explicit GLParticleBackend(class Actor* owner);
    ~GLParticleBackend() override;

    void Init(const ParticleDesc& desc) override;
    bool InitFromFile(const std::string& filePath) override;

    void Start() override;
    void Stop() override;
    void Reset() override;

    void Update(float deltaTime) override;

    void GatherRenderItems(RenderQueue& outQueue,
                           const VisualComponent& host) override;

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
    GLShader*   mUpdateShader { nullptr };
    std::string mUpdatePipelineName;
    std::string mRenderPipelineName;

    ParticleDesc mDesc {};
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
