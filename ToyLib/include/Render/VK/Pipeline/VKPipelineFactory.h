#pragma once

#include "Render/VK/Pipeline/VKPipeline.h"
#include <unordered_map>
#include <memory>

namespace toy
{

class VKPipelineFactory
{
public:
    void Shutdown(); // 全破棄

    std::shared_ptr<VKPipeline> GetOrCreate(VkDevice device,
                                            const std::string& key,
                                            const VKPipelineDesc& desc);

private:
    std::unordered_map<std::string, std::shared_ptr<VKPipeline>> mCache;
};

} // namespace toy
