// Render/VK/VKPipeline.h
#pragma once
#include <vulkan/vulkan.h>

namespace toy {

class VKPipeline
{
public:
    // debug (optional)
    const char* debugName { nullptr };

    VkPipeline       pipeline       { VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout { VK_NULL_HANDLE };

    // Descriptor set layouts
    // - Sprite: setLayout0 だけ使用
    // - Mesh  : setLayout0(Material) だけでまず動かす
    // - 将来  : setLayout1(Scene) 等を追加していける
    VkDescriptorSetLayout setLayout0 { VK_NULL_HANDLE };
    VkDescriptorSetLayout setLayout1 { VK_NULL_HANDLE }; // optional (future)
    VkDescriptorSetLayout setLayout2 { VK_NULL_HANDLE }; // optional (future)

    // which renderpass it was created for (optional but safe)
    VkRenderPass renderPass { VK_NULL_HANDLE };
    
};

} // namespace toy
