#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKUtil.h"

#include <iostream>

namespace toy {

// VKRenderer_Shader.cpp みたいな場所に
std::string VKRenderer::MakeVKSpvPath(const std::string& filename) const
{
    // mShaderPath が "ToyLib/Shaders/" みたいに末尾 / がある運用でも壊れないように
    std::string base = mShaderPath;
    if (!base.empty() && base.back() != '/' && base.back() != '\\')
    {
        base += "/";
    }
    return base + "VK/spv/" + filename;
}

VkShaderModule VKRenderer::LoadShaderModule(const std::string& spvFile)
{
    std::vector<uint8_t> code;
    const std::string path = MakeVKSpvPath(spvFile);

    if (!toy::vkutil::ReadFileBinary(path, code))
    {
        std::cerr << "[VKRenderer] Failed to read spv: " << path << "\n";
        return VK_NULL_HANDLE;
    }
    VkShaderModule mod = toy::vkutil::CreateShaderModule(mDevice, code);
    if (!mod)
    {
        std::cerr << "[VKRenderer] Failed to create shader module: " << path << "\n";
    }
    return mod;
}

bool VKRenderer::CreatePipelineLayout_Triangle()
{
    if (mPipeLayoutTriangle) return true;

    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = 0;
    ci.pushConstantRangeCount = 0;

    VkResult r = vkCreatePipelineLayout(mDevice, &ci, nullptr, &mPipeLayoutTriangle);
    return (r == VK_SUCCESS && mPipeLayoutTriangle);
}

bool VKRenderer::CreatePipeline_Triangle()
{
    if (mPipelineTriangle) return true;
    if (!CreatePipelineLayout_Triangle()) return false;

    VkShaderModule vs = LoadShaderModule("Triangle.vert.spv");
    VkShaderModule fs = LoadShaderModule("Triangle.frag.spv");
    if (!vs || !fs) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName  = "main";

    // Vertex input: none
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_FRONT_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // ToyLib方針に合わせるならここ
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAtt.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cbAtt;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkDynamicState dyns[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyns;

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState   = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState      = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState   = &ms;
    gp.pDepthStencilState  = &ds;
    gp.pColorBlendState    = &cb;
    gp.pDynamicState       = &dyn;

    gp.layout     = mPipeLayoutTriangle;
    gp.renderPass = mRenderPass;
    gp.subpass    = 0;

    VkResult r = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gp, nullptr, &mPipelineTriangle);

    vkDestroyShaderModule(mDevice, vs, nullptr);
    vkDestroyShaderModule(mDevice, fs, nullptr);

    return (r == VK_SUCCESS && mPipelineTriangle);
}

}
