// Render/VK/VKRenderer_CreateMeshPipelines.cpp（Mesh 部分）

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include "Utils/MathUtil.h" // Matrix4
#include <vector>
#include <iostream>

namespace toy
{

static void GetMeshVertexInputState(
    VkPipelineVertexInputStateCreateInfo& outInfo,
    std::vector<VkVertexInputBindingDescription>& bindings,
    std::vector<VkVertexInputAttributeDescription>& attrs)
{
    bindings.clear();
    attrs.clear();

    VkVertexInputBindingDescription b{};
    b.binding   = 0;
    b.stride    = sizeof(float) * (3 + 3 + 2);
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings.push_back(b);

    VkVertexInputAttributeDescription a0{};
    a0.location = 0; a0.binding = 0;
    a0.format   = VK_FORMAT_R32G32B32_SFLOAT;
    a0.offset   = 0;
    attrs.push_back(a0);

    VkVertexInputAttributeDescription a1{};
    a1.location = 1; a1.binding = 0;
    a1.format   = VK_FORMAT_R32G32B32_SFLOAT;
    a1.offset   = sizeof(float) * 3;
    attrs.push_back(a1);

    VkVertexInputAttributeDescription a2{};
    a2.location = 2; a2.binding = 0;
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

bool VKRenderer::CreateMeshPipeline()
{
    std::vector<uint8_t> vertCode;
    std::vector<uint8_t> fragCode;

    if (!vkutil::ReadFileBinary("ToyLib/Shaders/VK/spv/Mesh_Phong.vert.spv", vertCode))
    {
        std::cerr << "Mesh_Phong.vert.spv load failed\n";
        return false;
    }
    if (!vkutil::ReadFileBinary("ToyLib/Shaders/VK/spv/Mesh_Phong.frag.spv", fragCode))
    {
        std::cerr << "Mesh_Phong.frag.spv load failed\n";
        return false;
    }

    VkShaderModule vertModule = vkutil::CreateShaderModule(mDevice, vertCode);
    VkShaderModule fragModule = vkutil::CreateShaderModule(mDevice, fragCode);
    if (!vertModule || !fragModule)
    {
        std::cerr << "Mesh shader module create failed\n";
        if (vertModule) vkDestroyShaderModule(mDevice, vertModule, nullptr);
        if (fragModule) vkDestroyShaderModule(mDevice, fragModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    GetMeshVertexInputState(vi, bindings, attrs);

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = VK_CULL_MODE_BACK_BIT;
    rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

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

    VkDynamicState dynStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    //========================================================
    // Descriptor set layouts
    //========================================================
    // set0: Material (Diffuse texture sampler)
    VkDescriptorSetLayout set0 = VK_NULL_HANDLE;
    {
        std::vector<VkDescriptorSetLayoutBinding> b;
        b.push_back(vkutil::MakeBinding_CombinedImageSampler(
            0, VK_SHADER_STAGE_FRAGMENT_BIT));

        set0 = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (!set0) { std::cerr << "Mesh set0(Texture) layout failed\n"; return false; }
    }

    // set1: Scene/Common (UBO群)  ★bindingを詰める（0..3）
    VkDescriptorSetLayout set1 = VK_NULL_HANDLE;
    {
        std::vector<VkDescriptorSetLayoutBinding> b;

        // 0: WorldCommon UBO (VS+FS)
        b.push_back(vkutil::MakeBinding_UBO(
            0, VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 1));

        // 1: MaterialParams UBO (FS)
        b.push_back(vkutil::MakeBinding_UBO(
            1, VK_SHADER_STAGE_FRAGMENT_BIT, 1));

        // 2: DirLight UBO (FS)
        b.push_back(vkutil::MakeBinding_UBO(
            2, VK_SHADER_STAGE_FRAGMENT_BIT, 1));

        // 3: PointLightBlock UBO (FS)
        b.push_back(vkutil::MakeBinding_UBO(
            3, VK_SHADER_STAGE_FRAGMENT_BIT, 1));

        set1 = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (!set1) { std::cerr << "Mesh set1(Scene) layout failed\n"; return false; }
    }

    //========================================================
    // Push constant
    //  - 方針Bでは viewProj は set1(WorldCommon) に置く想定
    //  - PushConstant は world だけにするのが自然
    //    (shader側もそれに合わせる)
    //========================================================
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset     = 0;
    pc.size       = sizeof(Matrix4); // world only

    VkDescriptorSetLayout setLayouts[2] = { set0, set1 };

    VkPipelineLayoutCreateInfo pl{};
    pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount         = 2;
    pl.pSetLayouts            = setLayouts;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges    = &pc;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(mDevice, &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        std::cerr << "Mesh pipeline layout failed\n";
        vkDestroyDescriptorSetLayout(mDevice, set1, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, set0, nullptr);
        vkDestroyShaderModule(mDevice, vertModule, nullptr);
        vkDestroyShaderModule(mDevice, fragModule, nullptr);
        return false;
    }

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
    gp.renderPass          = mRenderPass;
    gp.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline) != VK_SUCCESS)
    {
        std::cerr << "Mesh pipeline create failed\n";
        vkDestroyPipelineLayout(mDevice, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, set1, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, set0, nullptr);
        vkDestroyShaderModule(mDevice, vertModule, nullptr);
        vkDestroyShaderModule(mDevice, fragModule, nullptr);
        return false;
    }

    auto p = std::make_unique<VKPipeline>();
    p->debugName      = "Mesh";
    p->pipeline       = pipeline;
    p->pipelineLayout = pipelineLayout;
    p->setLayout0     = set0;
    p->setLayout1     = set1;
    p->renderPass     = mRenderPass;

    mPipelines["Mesh"] = std::move(p);

    vkDestroyShaderModule(mDevice, vertModule, nullptr);
    vkDestroyShaderModule(mDevice, fragModule, nullptr);

    return true;
}

} // namespace toy
