// Render/VK/VKPipeline.h
#pragma once
#include <vulkan/vulkan.h>

namespace toy {

struct VKPipeline
{
    VkPipeline       pipeline { VK_NULL_HANDLE };
    VkPipelineLayout layout   { VK_NULL_HANDLE };

    // もしUI用で descriptor が固定ならここに持ってもOK
    VkDescriptorSetLayout descSetLayout { VK_NULL_HANDLE };
};

} // namespace toy
