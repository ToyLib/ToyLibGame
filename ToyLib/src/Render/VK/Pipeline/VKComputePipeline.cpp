#include "Render/VK/Pipeline/VKComputePipeline.h"

#include <fstream>
#include <iostream>
#include <vector>

namespace toy
{

VKComputePipeline::~VKComputePipeline()
{
    Destroy();
}

//======================================================================
// Destroy
//======================================================================
void VKComputePipeline::Destroy()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return;
    }

    if (mPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(mDevice, mPipeline, nullptr);
        mPipeline = VK_NULL_HANDLE;
    }

    if (mLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(mDevice, mLayout, nullptr);
        mLayout = VK_NULL_HANDLE;
    }

    for (auto& sl : mSetLayouts)
    {
        if (sl != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(mDevice, sl, nullptr);
            sl = VK_NULL_HANDLE;
        }
    }
    mSetLayouts.clear();

    mDevice = VK_NULL_HANDLE;
}

//======================================================================
// Bind
//======================================================================
void VKComputePipeline::Bind(VkCommandBuffer cmd) const
{
    if (cmd == VK_NULL_HANDLE || mPipeline == VK_NULL_HANDLE)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);
}

//======================================================================
// LoadShaderModule
//======================================================================
VkShaderModule VKComputePipeline::LoadShaderModule(VkDevice device, const std::string& spvPath)
{
    if (device == VK_NULL_HANDLE || spvPath.empty())
    {
        return VK_NULL_HANDLE;
    }

    std::ifstream ifs(spvPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        std::cerr << "[VKComputePipeline] Failed to open shader: " << spvPath << "\n";
        return VK_NULL_HANDLE;
    }

    const std::streamsize size = ifs.tellg();
    if (size <= 0)
    {
        std::cerr << "[VKComputePipeline] Shader file is empty: " << spvPath << "\n";
        return VK_NULL_HANDLE;
    }

    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(static_cast<size_t>(size));
    if (!ifs.read(bytes.data(), size))
    {
        std::cerr << "[VKComputePipeline] Failed to read shader: " << spvPath << "\n";
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = bytes.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(bytes.data());

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
    {
        std::cerr << "[VKComputePipeline] vkCreateShaderModule failed: " << spvPath << "\n";
        return VK_NULL_HANDLE;
    }

    return mod;
}

} // namespace toy
