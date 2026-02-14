// Render/VK/VKRenderer_CreateMeshPipelines.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include <vector>
#include <iostream>

namespace toy
{

// ------------------------------------------------------------
// Vertex layout（GL の location に合わせる）
// layout(location=0) inPosition
// layout(location=1) inNormal
// layout(location=2) inTexCoord
// ------------------------------------------------------------
static void GetMeshVertexInputState(
    VkPipelineVertexInputStateCreateInfo& outInfo,
    std::vector<VkVertexInputBindingDescription>& bindings,
    std::vector<VkVertexInputAttributeDescription>& attrs)
{
    bindings.clear();
    attrs.clear();

    // binding 0: 1 vertex = 1 struct
    VkVertexInputBindingDescription b{};
    b.binding   = 0;
    b.stride    = sizeof(float) * (3 + 3 + 2); // pos + normal + uv
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings.push_back(b);

    // location 0 : position
    VkVertexInputAttributeDescription a0{};
    a0.location = 0;
    a0.binding  = 0;
    a0.format   = VK_FORMAT_R32G32B32_SFLOAT;
    a0.offset   = 0;
    attrs.push_back(a0);

    // location 1 : normal
    VkVertexInputAttributeDescription a1{};
    a1.location = 1;
    a1.binding  = 0;
    a1.format   = VK_FORMAT_R32G32B32_SFLOAT;
    a1.offset   = sizeof(float) * 3;
    attrs.push_back(a1);

    // location 2 : texcoord
    VkVertexInputAttributeDescription a2{};
    a2.location = 2;
    a2.binding  = 0;
    a2.format   = VK_FORMAT_R32G32_SFLOAT;
    a2.offset   = sizeof(float) * 6;
    attrs.push_back(a2);

    outInfo = {};
    outInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    outInfo.vertexBindingDescriptionCount   = (uint32_t)bindings.size();
    outInfo.pVertexBindingDescriptions      = bindings.data();
    outInfo.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    outInfo.pVertexAttributeDescriptions    = attrs.data();
}

// ------------------------------------------------------------
// Create Mesh Pipeline
// ------------------------------------------------------------
bool VKRenderer::CreateMeshPipeline()
{
    // ------------------------------------------
    // Shader modules
    // ------------------------------------------
    std::vector<uint8_t> vertCode;
    std::vector<uint8_t> fragCode;

    if (!vkutil::ReadFileBinary("ToyLib/Shaders/VK/spv/Mesh_Phong.vert.spv", vertCode))
    {
        std::cerr << "Mesh.vert.spv load failed\n";
        return false;
    }
    if (!vkutil::ReadFileBinary("ToyLib/Shaders/VK/spv/Mesh_Phong.frag.spv", fragCode))
    {
        std::cerr << "Mesh.frag.spv load failed\n";
        return false;
    }

    VkShaderModule vertModule = vkutil::CreateShaderModule(mDevice, vertCode);
    VkShaderModule fragModule = vkutil::CreateShaderModule(mDevice, fragCode);

    // ------------------------------------------
    // Shader stages
    // ------------------------------------------
    VkPipelineShaderStageCreateInfo stages[2]{};

    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName  = "main";

    // ------------------------------------------
    // Vertex input
    // ------------------------------------------
    VkPipelineVertexInputStateCreateInfo vi{};
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    GetMeshVertexInputState(vi, bindings, attrs);

    // ------------------------------------------
    // Input assembly
    // ------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    // ------------------------------------------
    // Viewport / Scissor（dynamic）
    // ------------------------------------------
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    // ------------------------------------------
    // Rasterizer
    // ------------------------------------------
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = VK_CULL_MODE_BACK_BIT;
    rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE; // GLと合わせる
    rs.lineWidth               = 1.0f;
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable         = VK_FALSE;

    // ------------------------------------------
    // Multisample
    // ------------------------------------------
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ------------------------------------------
    // Depth stencil
    // ------------------------------------------
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable       = VK_TRUE;
    ds.depthWriteEnable      = VK_TRUE;
    ds.depthCompareOp        = VK_COMPARE_OP_LESS;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable     = VK_FALSE;

    // ------------------------------------------
    // Color blend
    // ------------------------------------------
    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    cbAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAttach;

    // ------------------------------------------
    // Dynamic state
    // ------------------------------------------
    VkDynamicState dynStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    // ------------------------------------------
    // Descriptor set layout（set0 = material）
    // ------------------------------------------
    VkDescriptorSetLayout setLayout =
        vkutil::CreateSetLayout_CombinedImageSampler(
            mDevice,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT);

    // ------------------------------------------
    // Push constants（world + viewProj）
    // ------------------------------------------
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset     = 0;
    pc.size = sizeof(Matrix4);

    // ------------------------------------------
    // Pipeline layout
    // ------------------------------------------
    VkPipelineLayoutCreateInfo pl{};
    pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount         = 1;
    pl.pSetLayouts            = &setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges    = &pc;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(mDevice, &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        std::cerr << "Mesh pipeline layout failed\n";
        return false;
    }

    // ------------------------------------------
    // Graphics pipeline
    // ------------------------------------------
    VkGraphicsPipelineCreateInfo gp{};
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
    gp.layout              = pipelineLayout;
    gp.renderPass          = mRenderPass; // ★Rendererが持ってる前提
    gp.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline) != VK_SUCCESS)
    {
        std::cerr << "Mesh pipeline create failed\n";
        return false;
    }

    // ------------------------------------------
    // 保存
    // ------------------------------------------
    auto p = std::make_unique<VKPipeline>();
    p->pipeline       = pipeline;
    p->pipelineLayout = pipelineLayout;
    p->setLayout0     = setLayout;
    p->renderPass     = mRenderPass;

    mPipelines["Mesh"] = std::move(p);

    // shader module は pipeline 作成後に破棄可能
    vkDestroyShaderModule(mDevice, vertModule, nullptr);
    vkDestroyShaderModule(mDevice, fragModule, nullptr);

    return true;
}

} // namespace toy
