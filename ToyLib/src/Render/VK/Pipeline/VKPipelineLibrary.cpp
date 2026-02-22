// Render/VK/Pipeline/VKPipelineLibrary.cpp
#include "Render/VK/Pipeline/VKPipelineLibrary.h"
#include <iostream>

namespace toy
{

bool VKPipelineLibrary::CreatePipeline(const std::string& name,
                                       VkDevice device,
                                       VkRenderPass renderPass,
                                       VkExtent2D extent,
                                       const VKPipelineDesc& desc)
{
    if (name.empty()) return false;

    auto p = std::make_shared<VKPipeline>();
    if (!p->Create(device, renderPass, extent, desc))
    {
        std::cerr << "[VKPipelineLibrary] CreatePipeline failed: " << name << "\n";
        return false;
    }

    mMap[name] = p;
    return true;
}

VKPipeline* VKPipelineLibrary::Get(const std::string& name) const
{
    auto it = mMap.find(name);
    if (it == mMap.end()) return nullptr;
    return it->second.get();
}

void VKPipelineLibrary::DestroyAll()
{
    for (auto& kv : mMap)
    {
        if (kv.second) kv.second->Destroy();
    }
    mMap.clear();
}

} // namespace toy
