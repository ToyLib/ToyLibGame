#include "Render/VK/VKRenderer.h"
#include "Graphics/Effect/VKParticleBackend.h"

#include <algorithm>

namespace toy {

//--------------------------------------------------------------
// EnqueueParticleCompute
//--------------------------------------------------------------
void VKRenderer::EnqueueParticleCompute(VKParticleBackend* backend, float deltaTime)
{
    if (!backend)
    {
        return;
    }

    ParticleComputeJob job{};
    job.backend   = backend;
    job.deltaTime = deltaTime;

    mParticleComputeJobs.push_back(job);
}

//--------------------------------------------------------------
// DequeueParticleCompute
//--------------------------------------------------------------
void VKRenderer::DequeueParticleCompute(VKParticleBackend* backend)
{
    if (!backend)
    {
        return;
    }

    mParticleComputeJobs.erase(
        std::remove_if(
            mParticleComputeJobs.begin(),
            mParticleComputeJobs.end(),
            [backend](const ParticleComputeJob& job)
            {
                return job.backend == backend;
            }),
        mParticleComputeJobs.end());
}

//--------------------------------------------------------------
// RecordQueuedParticleComputes
//--------------------------------------------------------------
void VKRenderer::RecordQueuedParticleComputes(VkCommandBuffer cmd)
{
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    for (const auto& job : mParticleComputeJobs)
    {
        if (!job.backend)
        {
            continue;
        }

        job.backend->UpdateParticlesCompute(cmd, job.deltaTime);
    }

    mParticleComputeJobs.clear();
}

} // namespace toy
