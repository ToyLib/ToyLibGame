#include "Render/VK/Pipeline/VKPipelineFactory.h"
#include <iostream>

namespace toy
{

void VKPipelineFactory::Shutdown()
{
    mCache.clear();
}

std::shared_ptr<VKPipeline> VKPipelineFactory::GetOrCreate(
    VkDevice device,
    const std::string& key,
    const VKPipelineDesc& desc)
{
    auto it = mCache.find(key);
    if (it != mCache.end())
    {
        return it->second;
    }

    auto p = std::make_shared<VKPipeline>();
    if (!p->Create(device, desc))
    {
        std::cerr << "[VKPipelineFactory] Create failed: " << key << "\n";
        return nullptr;
    }

    mCache.emplace(key, p);
    return p;
}

} // namespace toy
