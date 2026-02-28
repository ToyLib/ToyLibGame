//======================================================================
// Render/VK/Pipeline/VKPipelineLibrary.cpp
//======================================================================
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
    if (name.empty())
    {
        return false;
    }
    if (!device || !renderPass || extent.width == 0 || extent.height == 0)
    {
        std::cerr << "[VKPipelineLibrary] CreatePipeline invalid args: " << name << "\n";
        return false;
    }

    // ------------------------------------------------------------
    // 安全のため「新規に作って成功したら差し替え」
    // （失敗時に既存pipelineを壊さない）
    // ------------------------------------------------------------
    auto fresh = std::make_shared<VKPipeline>();
    if (!fresh->Create(device, renderPass, extent, desc))
    {
        std::cerr << "[VKPipelineLibrary] CreatePipeline failed: " << name << "\n";
        return false;
    }

    // 既存があれば明示破棄（dtorでも行うが、意図を明確に）
    if (auto it = mMap.find(name); it != mMap.end())
    {
        if (it->second)
        {
            it->second->Destroy();
        }
    }

    mMap[name] = fresh;
    return true;
}

VKPipeline* VKPipelineLibrary::Get(const std::string& name) const
{
    auto it = mMap.find(name);
    if (it == mMap.end())
    {
        return nullptr;
    }
    return it->second.get();
}

void VKPipelineLibrary::DestroyAll()
{
    for (auto& kv : mMap)
    {
        if (kv.second)
        {
            kv.second->Destroy();
        }
    }
    mMap.clear();
}

} // namespace toy
