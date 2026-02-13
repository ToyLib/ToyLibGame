// Render/VK/VKPipeline.h
#pragma once
#include <vulkan/vulkan.h>

namespace toy {

class VKPipeline
{
public:
    VkPipeline       pipeline       { VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout { VK_NULL_HANDLE };

    // 2D(Sprite) なら descriptor set layout もここに持つと便利
    VkDescriptorSetLayout setLayout { VK_NULL_HANDLE };

    // もし将来 RenderPass が複数になるなら「どのRenderPass向けか」も保持
    VkRenderPass renderPass { VK_NULL_HANDLE };
};

} // namespace toy
