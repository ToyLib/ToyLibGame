#pragma once

#include "Render/RenderEnums.h"
#include "Render/RenderItem.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace toy
{

struct VKPipelineKey
{
    RenderItemType type {};
    VkRenderPass   renderPass { VK_NULL_HANDLE };

    bool operator==(const VKPipelineKey& r) const
    {
        return type == r.type &&
               renderPass == r.renderPass;
    }
};

class VKPipeline
{
public:
    VKPipeline() = default;
    ~VKPipeline();

    bool Create(
        VkDevice device,
        const VKPipelineKey& key,
        VkExtent2D extent,
        const std::string& vertSpvPath,
        const std::string& fragSpvPath,
        bool enableDepth);

    void Destroy();

    VkPipeline       Get() const { return mPipeline; }
    VkPipelineLayout GetLayout() const { return mLayout; }

private:
    static bool ReadFileBinary(const std::string& path, std::vector<uint8_t>& out);

    static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& code);

    bool CreateLayout(VkDevice device);
    bool CreateGraphicsPipeline(VkDevice device,
                                const VKPipelineKey& key,
                                VkExtent2D extent,
                                const std::string& vertSpvPath,
                                const std::string& fragSpvPath,
                                bool enableDepth);

private:
    VkDevice         mDevice   { VK_NULL_HANDLE };
    VkPipeline       mPipeline { VK_NULL_HANDLE };
    VkPipelineLayout mLayout   { VK_NULL_HANDLE };
};

} // namespace toy
