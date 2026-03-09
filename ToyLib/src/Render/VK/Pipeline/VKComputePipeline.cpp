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

bool VKComputePipeline::Create(VkDevice device, const VKComputePipelineDesc& desc)
{
    Destroy();

    if (device == VK_NULL_HANDLE)
    {
        std::cerr << "[VKComputePipeline] Create failed: device is null\n";
        return false;
    }

    if (desc.csPath.empty())
    {
        std::cerr << "[VKComputePipeline] Create failed: csPath is empty\n";
        return false;
    }

    mDevice = device;

    //----------------------------------------------------------
    // Compute shader module
    //----------------------------------------------------------
    VkShaderModule cs = LoadShaderModule(device, desc.csPath);
    if (cs == VK_NULL_HANDLE)
    {
        std::cerr << "[VKComputePipeline] LoadShaderModule failed: " << desc.csPath << "\n";
        return false;
    }

    //----------------------------------------------------------
    // Descriptor set layouts
    //----------------------------------------------------------
    std::vector<VkDescriptorSetLayout> setLayouts;
    mSetLayouts.clear();

    if (!desc.setLayouts.empty())
    {
        uint32_t maxSet = 0;
        for (const auto& sl : desc.setLayouts)
        {
            maxSet = std::max(maxSet, sl.set);
        }

        mSetLayouts.resize(maxSet + 1, VK_NULL_HANDLE);
        setLayouts.resize(maxSet + 1, VK_NULL_HANDLE);

        for (const auto& sl : desc.setLayouts)
        {
            std::vector<VkDescriptorSetLayoutBinding> bindingsDesc;
            bindingsDesc.reserve(sl.bindings.size());

            for (const auto& b : sl.bindings)
            {
                VkDescriptorSetLayoutBinding vkbind{};
                vkbind.binding            = b.binding;
                vkbind.descriptorType     = b.type;
                vkbind.descriptorCount    = b.count;
                vkbind.stageFlags         = b.stages;
                vkbind.pImmutableSamplers = nullptr;
                bindingsDesc.push_back(vkbind);
            }

            VkDescriptorSetLayoutCreateInfo linfo{};
            linfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            linfo.bindingCount = static_cast<uint32_t>(bindingsDesc.size());
            linfo.pBindings    = bindingsDesc.empty() ? nullptr : bindingsDesc.data();

            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            VkResult lres = vkCreateDescriptorSetLayout(device, &linfo, nullptr, &layout);
            if (lres != VK_SUCCESS)
            {
                std::cerr << "[VKComputePipeline] vkCreateDescriptorSetLayout failed: " << lres << "\n";
                vkDestroyShaderModule(device, cs, nullptr);
                Destroy();
                return false;
            }

            mSetLayouts[sl.set] = layout;
            setLayouts[sl.set]  = layout;
        }
    }

    //----------------------------------------------------------
    // Push constant ranges
    //----------------------------------------------------------
    std::vector<VkPushConstantRange> pcRanges;
    pcRanges.reserve(desc.pushConstants.size());

    for (const auto& pc : desc.pushConstants)
    {
        VkPushConstantRange r{};
        r.stageFlags = pc.stages;
        r.offset     = pc.offset;
        r.size       = pc.size;
        pcRanges.push_back(r);
    }

    //----------------------------------------------------------
    // Pipeline layout
    //----------------------------------------------------------
    VkPipelineLayoutCreateInfo pl{};
    pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    pl.pSetLayouts            = setLayouts.empty() ? nullptr : setLayouts.data();
    pl.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size());
    pl.pPushConstantRanges    = pcRanges.empty() ? nullptr : pcRanges.data();

    if (vkCreatePipelineLayout(device, &pl, nullptr, &mLayout) != VK_SUCCESS)
    {
        std::cerr << "[VKComputePipeline] vkCreatePipelineLayout failed\n";
        vkDestroyShaderModule(device, cs, nullptr);
        Destroy();
        return false;
    }

    //----------------------------------------------------------
    // Compute stage
    //----------------------------------------------------------
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = cs;
    stage.pName  = "main";

    //----------------------------------------------------------
    // Compute pipeline
    //----------------------------------------------------------
    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = mLayout;

    VkResult vr = vkCreateComputePipelines(
        device,
        VK_NULL_HANDLE,
        1,
        &ci,
        nullptr,
        &mPipeline);

    vkDestroyShaderModule(device, cs, nullptr);

    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKComputePipeline] vkCreateComputePipelines failed: " << vr << "\n";
        Destroy();
        return false;
    }

    return true;
}

} // namespace toy
