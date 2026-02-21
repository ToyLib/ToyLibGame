#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/VKUtil.h" // ReadFileBinary / CreateShaderModule を使う想定

#include <iostream>

namespace toy
{

static VkPipelineColorBlendAttachmentState MakeBlendState(bool enableAlphaBlend)
{
    VkPipelineColorBlendAttachmentState a{};
    a.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    if (!enableAlphaBlend)
    {
        a.blendEnable = VK_FALSE;
        return a;
    }

    // standard alpha blend
    a.blendEnable         = VK_TRUE;
    a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    a.colorBlendOp        = VK_BLEND_OP_ADD;
    a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    a.alphaBlendOp        = VK_BLEND_OP_ADD;
    return a;
}

bool VKPipeline::CreateLayout(const VKPipelineDesc& desc)
{
    VkPipelineLayoutCreateInfo lci{};
    lci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lci.setLayoutCount         = (uint32_t)desc.setLayouts.size();
    lci.pSetLayouts            = desc.setLayouts.empty() ? nullptr : desc.setLayouts.data();
    lci.pushConstantRangeCount = (uint32_t)desc.pushConstants.size();
    lci.pPushConstantRanges    = desc.pushConstants.empty() ? nullptr : desc.pushConstants.data();

    const VkResult r = vkCreatePipelineLayout(mDevice, &lci, nullptr, &mLayout);
    if (r != VK_SUCCESS || mLayout == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] vkCreatePipelineLayout failed: " << r << "\n";
        return false;
    }
    return true;
}

bool VKPipeline::Create(VkDevice device, const VKPipelineDesc& desc)
{
    Destroy();
    mDevice = device;

    if (!mDevice || desc.renderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] Create failed: device/renderPass invalid\n";
        return false;
    }

    // --- load spv
    std::vector<uint8_t> vcode;
    std::vector<uint8_t> fcode;

    if (!toy::vkutil::ReadFileBinary(desc.vertSpvPath, vcode))
    {
        std::cerr << "[VKPipeline] Failed to read vert: " << desc.vertSpvPath << "\n";
        return false;
    }
    if (!toy::vkutil::ReadFileBinary(desc.fragSpvPath, fcode))
    {
        std::cerr << "[VKPipeline] Failed to read frag: " << desc.fragSpvPath << "\n";
        return false;
    }

    VkShaderModule vmod = toy::vkutil::CreateShaderModule(mDevice, vcode);
    VkShaderModule fmod = toy::vkutil::CreateShaderModule(mDevice, fcode);
    if (!vmod || !fmod)
    {
        if (vmod) vkDestroyShaderModule(mDevice, vmod, nullptr);
        if (fmod) vkDestroyShaderModule(mDevice, fmod, nullptr);
        return false;
    }

    if (!CreateLayout(desc))
    {
        vkDestroyShaderModule(mDevice, vmod, nullptr);
        vkDestroyShaderModule(mDevice, fmod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vmod;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fmod;
    stages[1].pName  = "main";

    // --- vertex input
    VkPipelineVertexInputStateCreateInfo vis{};
    vis.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount   = (uint32_t)desc.bindings.size();
    vis.pVertexBindingDescriptions      = desc.bindings.empty() ? nullptr : desc.bindings.data();
    vis.vertexAttributeDescriptionCount = (uint32_t)desc.attributes.size();
    vis.pVertexAttributeDescriptions    = desc.attributes.empty() ? nullptr : desc.attributes.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology               = desc.topology;
    ia.primitiveRestartEnable = VK_FALSE;

    // --- viewport/scissor dynamic
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = desc.cullMode;
    rs.frontFace               = desc.frontFace;
    rs.depthBiasEnable         = VK_FALSE;
    rs.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = desc.depthTest;
    ds.depthWriteEnable = desc.depthWrite;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState att = MakeBlendState(desc.enableAlphaBlend);

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &att;

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vis;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = mLayout;
    pci.renderPass          = desc.renderPass;
    pci.subpass             = desc.subpass;

    const VkResult r = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pci, nullptr, &mPipeline);

    vkDestroyShaderModule(mDevice, vmod, nullptr);
    vkDestroyShaderModule(mDevice, fmod, nullptr);

    if (r != VK_SUCCESS || mPipeline == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] vkCreateGraphicsPipelines failed: " << r << "\n";
        Destroy();
        return false;
    }

    return true;
}

void VKPipeline::Destroy()
{
    if (!mDevice) return;

    if (mPipeline)
    {
        vkDestroyPipeline(mDevice, mPipeline, nullptr);
        mPipeline = VK_NULL_HANDLE;
    }
    if (mLayout)
    {
        vkDestroyPipelineLayout(mDevice, mLayout, nullptr);
        mLayout = VK_NULL_HANDLE;
    }
    mDevice = VK_NULL_HANDLE;
}

} // namespace toy
