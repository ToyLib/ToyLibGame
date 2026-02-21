#include "Render/VK/VKPipeline.h"

#include <fstream>
#include <iostream>

namespace toy
{

VKPipeline::~VKPipeline()
{
    Destroy();
}

void VKPipeline::Destroy()
{
    if (mDevice != VK_NULL_HANDLE)
    {
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
    }
}

bool VKPipeline::ReadFileBinary(const std::string& path, std::vector<uint8_t>& out)
{
    out.clear();

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs)
    {
        std::cerr << "[VKPipeline] Failed to open: " << path << "\n";
        return false;
    }

    const std::streamsize size = ifs.tellg();
    if (size <= 0)
    {
        std::cerr << "[VKPipeline] Invalid size: " << path << "\n";
        return false;
    }

    out.resize((size_t)size);
    ifs.seekg(0, std::ios::beg);
    if (!ifs.read((char*)out.data(), size))
    {
        std::cerr << "[VKPipeline] Failed to read: " << path << "\n";
        return false;
    }

    return true;
}

VkShaderModule VKPipeline::CreateShaderModule(VkDevice device, const std::vector<uint8_t>& code)
{
    if (code.empty())
    {
        return VK_NULL_HANDLE;
    }
    if ((code.size() % 4) != 0)
    {
        std::cerr << "[VKPipeline] SPV size not multiple of 4\n";
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci {};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = (const uint32_t*)code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return mod;
}

bool VKPipeline::CreateLayout(VkDevice device)
{
    // push constant: vec4 color
    VkPushConstantRange range {};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset     = 0;
    range.size       = sizeof(float) * 4;

    VkPipelineLayoutCreateInfo ci {};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount         = 0;       // descriptor無し（まずは）
    ci.pSetLayouts            = nullptr;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &range;

    if (vkCreatePipelineLayout(device, &ci, nullptr, &mLayout) != VK_SUCCESS)
    {
        std::cerr << "[VKPipeline] vkCreatePipelineLayout failed\n";
        return false;
    }

    return true;
}

bool VKPipeline::Create(
    VkDevice device,
    const VKPipelineKey& key,
    VkExtent2D extent,
    const std::string& vertSpvPath,
    const std::string& fragSpvPath,
    bool enableDepth)
{
    Destroy();

    mDevice = device;

    if (!CreateLayout(device))
    {
        return false;
    }

    if (!CreateGraphicsPipeline(device, key, extent, vertSpvPath, fragSpvPath, enableDepth))
    {
        Destroy();
        return false;
    }

    return true;
}

bool VKPipeline::CreateGraphicsPipeline(
    VkDevice device,
    const VKPipelineKey& key,
    VkExtent2D /*extent*/,
    const std::string& vertSpvPath,
    const std::string& fragSpvPath,
    bool enableDepth)
{
    std::vector<uint8_t> vertCode;
    std::vector<uint8_t> fragCode;

    if (!ReadFileBinary(vertSpvPath, vertCode)) return false;
    if (!ReadFileBinary(fragSpvPath, fragCode)) return false;

    VkShaderModule vert = CreateShaderModule(device, vertCode);
    VkShaderModule frag = CreateShaderModule(device, fragCode);

    if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
    {
        if (vert) vkDestroyShaderModule(device, vert, nullptr);
        if (frag) vkDestroyShaderModule(device, frag, nullptr);
        std::cerr << "[VKPipeline] CreateShaderModule failed\n";
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] {};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName  = "main";

    // No vertex buffers: full screen triangle via gl_VertexIndex
    VkPipelineVertexInputStateCreateInfo vi {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia {};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp {};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs {};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = VK_CULL_MODE_NONE;
    rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms {};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAtt {};
    cbAtt.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    cbAtt.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb {};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAtt;

    VkPipelineDepthStencilStateCreateInfo ds {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    if (enableDepth)
    {
        ds.depthTestEnable  = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
    }
    else
    {
        ds.depthTestEnable  = VK_FALSE;
        ds.depthWriteEnable = VK_FALSE;
        ds.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
    }

    VkDynamicState dynStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dyn {};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = (uint32_t)std::size(dynStates);
    dyn.pDynamicStates    = dynStates;

    VkGraphicsPipelineCreateInfo gp {};
    gp.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount          = 2;
    gp.pStages             = stages;
    gp.pVertexInputState   = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState      = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState   = &ms;
    gp.pDepthStencilState  = &ds;
    gp.pColorBlendState    = &cb;
    gp.pDynamicState       = &dyn;
    gp.layout              = mLayout;
    gp.renderPass          = key.renderPass;
    gp.subpass             = 0;

    VkResult vr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &mPipeline);

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);

    if (vr != VK_SUCCESS || mPipeline == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] vkCreateGraphicsPipelines failed: " << vr << "\n";
        return false;
    }

    return true;
}

} // namespace toy
