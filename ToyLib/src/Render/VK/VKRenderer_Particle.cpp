#include "Render/VK/VKRenderer.h"
#include "Graphics/Effect/VKParticleBackend.h"


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
