#pragma once

#include "Graphics/Effect/IParticleBackend.h"

#include <vulkan/vulkan.h>
#include <string>

namespace toy
{

class VisualComponent;
class RenderQueue;

class VKParticleBackend : public IParticleBackend
{
public:
    explicit VKParticleBackend(class Actor* owner);
    ~VKParticleBackend() override;

    void Init(const ParticleDesc& desc) override;
    bool InitFromFile(const std::string& filePath) override;

    void Start() override;
    void Stop() override;
    void Reset() override;

    void Update(float deltaTime) override;

    void GatherRenderItems(RenderQueue& outQueue,
                           const VisualComponent& host) override;

private:
    struct ParticleGPU
    {
        float px, py, pz;
        float life;   // ← 4番目に life
        float vx, vy, vz;
        float pad;
    };

private:
    void InitIfNeeded();
    void ReleaseVK();

    void InitParticleBuffers(bool warmStart);
    void UpdateParticlesGPU(float deltaTime);

    VkBuffer CurrentSrcBuffer() const;
    VkBuffer CurrentDstBuffer() const;
    VkDeviceMemory CurrentSrcMemory() const;
    VkDeviceMemory CurrentDstMemory() const;

private:
    ParticleDesc mDesc {};
    bool  mInitialized { false };
    bool  mPingPong    { false };
    bool  mRunning     { false };

    float mComponentLifeAcc { 0.0f };
    float mTimeAcc          { 0.0f };

    bool mPendingHardReset { true };
    int  mSkipDrawFrames   { 2 };

    std::string mRenderPipelineName { "Particle" };
    std::string mUpdatePipelineName { "ParticleUpdateCompute" };

    VkBuffer       mParticleBufferA { VK_NULL_HANDLE };
    VkDeviceMemory mParticleMemoryA { VK_NULL_HANDLE };
    VkBuffer       mParticleBufferB { VK_NULL_HANDLE };
    VkDeviceMemory mParticleMemoryB { VK_NULL_HANDLE };
};

} // namespace toy
