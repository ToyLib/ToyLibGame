#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace toy
{

//==============================================================================
// VKPipelineDesc
//  - Pipeline state description (RTT + depth aware)
//==============================================================================
struct VKPipelineDesc
{
    bool depthTest  = true;
    bool depthWrite = true;

    bool blending   = false;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
};

//==============================================================================
// VKPipeline
//==============================================================================
class VKPipeline
{
public:
    VKPipeline() = default;
    ~VKPipeline();

    void Destroy();

    bool CreateGraphics(
        VkDevice device,
        VkRenderPass renderPass,     // ← ScenePass を渡す
        const VKPipelineDesc& desc,
        VkExtent2D extent,
        VkShaderModule vs,
        VkShaderModule fs
    );

    VkPipeline Get() const { return mPipeline; }
    VkPipelineLayout GetLayout() const { return mLayout; }

private:
    VkDevice mDevice = VK_NULL_HANDLE;

    VkPipelineLayout mLayout = VK_NULL_HANDLE;
    VkPipeline mPipeline     = VK_NULL_HANDLE;
};

}
