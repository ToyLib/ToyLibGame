//======================================================================
// VKRenderer_CreateSkinnedMeshPipeline.cpp
//  - set=0 : texture
//  - set=1 : common (既存Meshと共有)
//  - set=2 : bone palette（Skinned専用）
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include <iostream>
#include <vector>
#include <array>

namespace toy
{

static VkVertexInputBindingDescription MakeSkinnedBinding()
{
    VkVertexInputBindingDescription b{};
    b.binding   = 0;
    b.stride    = 64; // ★実データに合わせる
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return b;
}

static std::array<VkVertexInputAttributeDescription, 5> MakeSkinnedAttributes()
{
    std::array<VkVertexInputAttributeDescription, 5> a{};

    a[0].location = 0; a[0].binding = 0; a[0].format = VK_FORMAT_R32G32B32_SFLOAT;    a[0].offset = 0;
    a[1].location = 1; a[1].binding = 0; a[1].format = VK_FORMAT_R32G32B32_SFLOAT;    a[1].offset = 12;
    a[2].location = 2; a[2].binding = 0; a[2].format = VK_FORMAT_R32G32_SFLOAT;       a[2].offset = 24;
    a[3].location = 3; a[3].binding = 0; a[3].format = VK_FORMAT_R32G32B32A32_UINT;   a[3].offset = 32;
    a[4].location = 4; a[4].binding = 0; a[4].format = VK_FORMAT_R32G32B32A32_SFLOAT; a[4].offset = 48;

    return a;
}

bool VKRenderer::CreateSkinnedMeshPipeline()
{
    if (!mDevice || !mRenderPass)
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: device/renderPass not ready.\n";
        return false;
    }

    //========================================================
    // (0) set=0 (Texture sampler)
    //========================================================
    if (mWorldSetLayout0_Texture == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> b;
        b.push_back(vkutil::MakeBinding_CombinedImageSampler(
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            1));

        mWorldSetLayout0_Texture = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (mWorldSetLayout0_Texture == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: setLayout0(Texture) create failed\n";
            return false;
        }
    }

    //========================================================
    // (1) set=1 (Common) は既存Meshのものを必ず使う
    //========================================================
    if (mWorldSetLayout1_Common == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: mWorldSetLayout1_Common is null.\n";
        return false;
    }

    //========================================================
    // (2) set=2 (BonePalette)
    //   set=2 binding=0 : UBO BonePalette (VS)
    //========================================================
    if (mWorldSetLayout2_Bone == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> b;
        b.push_back(vkutil::MakeBinding_UBO(
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            1));

        mWorldSetLayout2_Bone = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (mWorldSetLayout2_Bone == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: setLayout2(Bone) create failed\n";
            return false;
        }
    }

    //========================================================
    // (3) 既存 SkinnedMesh pipeline があれば破棄
    //========================================================
    auto itOld = mPipelines.find("SkinnedMesh");
    if (itOld != mPipelines.end() && itOld->second)
    {
        VKPipeline* old = itOld->second.get();
        if (old->pipeline)       vkDestroyPipeline(mDevice, old->pipeline, nullptr);
        if (old->pipelineLayout) vkDestroyPipelineLayout(mDevice, old->pipelineLayout, nullptr);
        mPipelines.erase(itOld);
    }

    //========================================================
    // (4) pipeline object
    //========================================================
    auto pipeUP = std::make_unique<VKPipeline>();
    VKPipeline* pipe = pipeUP.get();
    pipe->debugName  = "SkinnedMesh";
    pipe->renderPass = mRenderPass;

    pipe->setLayout0 = mWorldSetLayout0_Texture; // set=0
    pipe->setLayout1 = mWorldSetLayout1_Common;  // set=1
    pipe->setLayout2 = mWorldSetLayout2_Bone;    // set=2 ★

    //========================================================
    // (5) pipeline layout : set0 + set1 + set2 + push constants
    //========================================================
    {
        VkDescriptorSetLayout setLayouts[3] =
        {
            pipe->setLayout0,
            pipe->setLayout1,
            pipe->setLayout2
        };

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset     = 0;
        pcr.size       = (uint32_t)sizeof(PushConstants_Mesh); // ★既存Meshと同じpush定義を使う

        VkPipelineLayoutCreateInfo lci{};
        lci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        lci.setLayoutCount         = 3;
        lci.pSetLayouts            = setLayouts;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges    = &pcr;
        
        if (vkCreatePipelineLayout(mDevice, &lci, nullptr, &pipe->pipelineLayout) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: vkCreatePipelineLayout failed.\n";
            return false;
        }
    }

    //========================================================
    // (6) shaders
    //========================================================
    const std::string vsPath = "ToyLib/Shaders/VK/spv/Skinned.vert.spv";
    const std::string fsPath = "ToyLib/Shaders/VK/spv/Mesh_Phong.frag.spv";

    std::vector<uint8_t> vsCode, fsCode;
    if (!vkutil::ReadFileBinary(vsPath, vsCode))
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: failed to read VS: " << vsPath << "\n";
        return false;
    }
    if (!vkutil::ReadFileBinary(fsPath, fsCode))
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: failed to read FS: " << fsPath << "\n";
        return false;
    }

    VkShaderModule vs = vkutil::CreateShaderModule(mDevice, vsCode);
    VkShaderModule fs = vkutil::CreateShaderModule(mDevice, fsCode);
    if (!vs || !fs)
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: CreateShaderModule failed.\n";
        if (vs) vkDestroyShaderModule(mDevice, vs, nullptr);
        if (fs) vkDestroyShaderModule(mDevice, fs, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName  = "main";

    //========================================================
    // (7) vertex input
    //========================================================
    const VkVertexInputBindingDescription binding = MakeSkinnedBinding();
    const auto attrs = MakeSkinnedAttributes();

    VkPipelineVertexInputStateCreateInfo vis{};
    vis.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount   = 1;
    vis.pVertexBindingDescriptions      = &binding;
    vis.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    vis.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkDynamicState dyns[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dyns;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_BACK_BIT;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dss{};
    dss.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dss.depthTestEnable  = VK_TRUE;
    dss.depthWriteEnable = VK_TRUE;
    dss.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState cbA{};
    cbA.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    cbA.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbA;

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount          = 2;
    gp.pStages             = stages;
    gp.pVertexInputState   = &vis;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState      = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState   = &ms;
    gp.pDepthStencilState  = &dss;
    gp.pColorBlendState    = &cb;
    gp.pDynamicState       = &dyn;
    gp.layout              = pipe->pipelineLayout;
    gp.renderPass          = mRenderPass;
    gp.subpass             = 0;

    VkResult r = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gp, nullptr, &pipe->pipeline);

    vkDestroyShaderModule(mDevice, vs, nullptr);
    vkDestroyShaderModule(mDevice, fs, nullptr);

    if (r != VK_SUCCESS || pipe->pipeline == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateGraphicsPipelines(SkinnedMesh) failed: " << r << "\n";
        vkDestroyPipelineLayout(mDevice, pipe->pipelineLayout, nullptr);
        pipe->pipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    mPipelines["SkinnedMesh"] = std::move(pipeUP);
    return true;
}

} // namespace toy
