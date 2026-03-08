#pragma once

#include "Graphics/VisualComponent.h"
#include "Graphics/Effect/ParticleDesc.h"

#include <memory>
#include <string>

namespace toy
{

class IParticleBackend;
class RenderQueue;

class ParticleComponent : public VisualComponent
{
public:
    using Desc = ParticleDesc;

public:
    ParticleComponent(class Actor* owner, int drawOrder = 20);
    ~ParticleComponent() override;

    void Init(const Desc& desc);
    bool InitFromFile(const std::string& filePath);

    void Start();
    void Stop();
    void Reset();

    void Update(float deltaTime) override;
    void GatherRenderItems(RenderQueue& outQueue) override;

private:
    Desc mDesc {};
    std::unique_ptr<IParticleBackend> mBackend;
};

} // namespace toy
