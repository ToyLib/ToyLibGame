// Render/VK/VKPipelineLibrary.h
#pragma once

#include "Render/VK/Pipeline/VKPipeline.h"
#include <unordered_map>
#include <memory>

namespace toy
{

class VKPipelineLibrary
{
public:
    bool CreatePipeline(const std::string& name,
                        VkDevice device,
                        VkRenderPass renderPass,
                        VkExtent2D extent,
                        const VKPipelineDesc& desc);

    VKPipeline* Get(const std::string& name) const;

    void DestroyAll();

private:
    std::unordered_map<std::string, std::shared_ptr<VKPipeline>> mMap;
};

} // namespace toy
