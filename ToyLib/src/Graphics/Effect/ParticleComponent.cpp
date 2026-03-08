#include "Graphics/Effect/ParticleComponent.h"

#include "Graphics/Effect/IParticleBackend.h"
#include "Graphics/Effect/GLParticleBackend.h"
#include "Graphics/Effect/VKParticleBackend.h"

#include "Render/RenderBackendState.h"

namespace toy
{

ParticleComponent::ParticleComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder, VisualLayer::Effect3D)
{
    auto& backend = RenderBackendState::Get();

    if (backend.IsGL())
    {
        mBackend = std::make_unique<GLParticleBackend>(owner);
    }
    else if (backend.IsVK())
    {
        mBackend = std::make_unique<VKParticleBackend>(owner);
    }
}

ParticleComponent::~ParticleComponent() = default;

void ParticleComponent::Init(const Desc& desc)
{
    mDesc = desc;

    SetBlendAdd(desc.additiveBlend);
    SetLayer(VisualLayer::Effect3D);

    if (mBackend)
    {
        mBackend->Init(mDesc);
    }
}

bool ParticleComponent::InitFromFile(const std::string& filePath)
{
    if (!mBackend)
    {
        return false;
    }
    return mBackend->InitFromFile(filePath);
}

void ParticleComponent::Start()
{
    if (mBackend)
    {
        mBackend->Start();
    }
}

void ParticleComponent::Stop()
{
    if (mBackend)
    {
        mBackend->Stop();
    }
}

void ParticleComponent::Reset()
{
    if (mBackend)
    {
        mBackend->Reset();
    }
}

void ParticleComponent::Update(float deltaTime)
{
    if (mBackend)
    {
        mBackend->Update(deltaTime);
    }
}

void ParticleComponent::GatherRenderItems(RenderQueue& outQueue)
{
    if (!IsVisible())
    {
        return;
    }

    if (mBackend)
    {
        mBackend->GatherRenderItems(outQueue, *this);
    }
}

} // namespace toy
