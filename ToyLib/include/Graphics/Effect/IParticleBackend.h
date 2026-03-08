#pragma once

#include "Graphics/Effect/ParticleDesc.h"

#include <string>

namespace toy
{

class Actor;
class VisualComponent;
class RenderQueue;

class IParticleBackend
{
public:
    explicit IParticleBackend(Actor* owner)
        : mOwner(owner)
    {
    }

    virtual ~IParticleBackend() = default;

    virtual void Init(const ParticleDesc& desc) = 0;
    virtual bool InitFromFile(const std::string& filePath) = 0;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Reset() = 0;

    virtual void Update(float deltaTime) = 0;

    virtual void GatherRenderItems(RenderQueue& outQueue,
                                   const VisualComponent& host) = 0;

protected:
    Actor* GetOwner() const
    {
        return mOwner;
    }

private:
    Actor* mOwner { nullptr };
};

} // namespace toy
