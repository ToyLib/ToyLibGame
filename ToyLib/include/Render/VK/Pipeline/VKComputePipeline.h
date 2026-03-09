#pragma once

#include "Render/VK/Pipeline/VKPipeline.h" // VKDescriptorSetLayoutDesc / VKPushConstantDesc を流用

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace toy
{

struct VKComputePipelineDesc
{
    // compute shader path (spv)
    std::string csPath;

    // pipeline layout
    std::vector<VKDescriptorSetLayoutDesc> setLayouts {};
    std::vector<VKPushConstantDesc>        pushConstants {};
};


class VKComputePipeline
{
public:
    VKComputePipeline() = default;
    ~VKComputePipeline();

    VKComputePipeline(const VKComputePipeline&) = delete;
    VKComputePipeline& operator=(const VKComputePipeline&) = delete;

    bool Create(VkDevice device, const VKComputePipelineDesc& desc);
    void Destroy();

    bool IsValid() const { return mPipeline != VK_NULL_HANDLE; }

    VkPipeline       GetPipeline()       const { return mPipeline; }
    VkPipelineLayout GetPipelineLayout() const { return mLayout;   }

    void Bind(VkCommandBuffer cmd) const;

    VkDescriptorSetLayout GetSetLayout(uint32_t setIndex) const
    {
        if (setIndex >= static_cast<uint32_t>(mSetLayouts.size()))
        {
            return VK_NULL_HANDLE;
        }
        return mSetLayouts[setIndex];
    }

    uint32_t GetSetLayoutCount() const
    {
        return static_cast<uint32_t>(mSetLayouts.size());
    }

private:
    static VkShaderModule LoadShaderModule(VkDevice device, const std::string& spvPath);

private:
    VkDevice         mDevice   { VK_NULL_HANDLE };
    VkPipeline       mPipeline { VK_NULL_HANDLE };
    VkPipelineLayout mLayout   { VK_NULL_HANDLE };

    std::vector<VkDescriptorSetLayout> mSetLayouts {};
};

} // namespace toy
