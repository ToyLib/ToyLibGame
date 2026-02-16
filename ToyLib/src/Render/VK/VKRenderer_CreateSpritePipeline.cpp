//======================================================================
// VKRenderer_CreateSpritePipeline.cpp (or inside VKRenderer.cpp)
//  - UI Sprite 用パイプライン作成
//  - set=0 binding=0 : CombinedImageSampler
//  - push constants  : mat4 world + mat4 viewProj + vec4 colorAlpha (144 bytes)
//  - vertex layout   : pos3 + normal3(dummy) + uv2  (8 floats)
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include <iostream>
#include <vector>
#include <array>

namespace toy {


// VertexArray の SpriteQuad(8 floats) を想定:
// pos3 + normal3(dummy) + uv2
static VkVertexInputBindingDescription MakeSpriteBinding()
{
    VkVertexInputBindingDescription b{};
    b.binding   = 0;
    b.stride    = sizeof(float) * 8;
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return b;
}

static std::array<VkVertexInputAttributeDescription, 3> MakeSpriteAttributes()
{
    std::array<VkVertexInputAttributeDescription, 3> a{};

    // location=0 vec3 inPosition
    a[0].location = 0;
    a[0].binding  = 0;
    a[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    a[0].offset   = 0;

    // location=1 vec3 inNormal (unused)
    a[1].location = 1;
    a[1].binding  = 0;
    a[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    a[1].offset   = sizeof(float) * 3;

    // location=2 vec2 inTexCoord
    a[2].location = 2;
    a[2].binding  = 0;
    a[2].format   = VK_FORMAT_R32G32_SFLOAT;
    a[2].offset   = sizeof(float) * 6;

    return a;
}

//--------------------------------------------------------------
// VKRenderer::CreateSpritePipeline
//--------------------------------------------------------------
bool VKRenderer::CreateSpritePipeline()
{
    if (!mDevice || !mRenderPass)
    {
        std::cerr << "[VKRenderer] CreateSpritePipeline: device/renderPass not ready.\n";
        return false;
    }

    //========================================================
    // (0) 共有 set0(Texture) を確保（Worldと共用）
    //========================================================
    if (mWorldSetLayout0_Texture == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> b;
        b.push_back(vkutil::MakeBinding_CombinedImageSampler(
            0, VK_SHADER_STAGE_FRAGMENT_BIT));

        mWorldSetLayout0_Texture = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (mWorldSetLayout0_Texture == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] setLayout0(Texture) create failed\n";
            return false;
        }
    }

    //========================================================
    // (1) 既存 Sprite pipeline を破棄（共有layoutは破棄しない）
    //========================================================
    auto itOld = mPipelines.find("Sprite");
    if (itOld != mPipelines.end() && itOld->second)
    {
        VKPipeline* old = itOld->second.get();
        if (old->pipeline)       vkDestroyPipeline(mDevice, old->pipeline, nullptr);
        if (old->pipelineLayout) vkDestroyPipelineLayout(mDevice, old->pipelineLayout, nullptr);

        old->pipeline       = VK_NULL_HANDLE;
        old->pipelineLayout = VK_NULL_HANDLE;

        // ★共有なので destroy しない
        old->setLayout0 = VK_NULL_HANDLE;

        mPipelines.erase(itOld);
    }
    // (1b) set=1 SpriteCommon layout を VKRenderer 側で共有生成
    if (mSpriteSetLayout1_Common == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> b;
        b.push_back(vkutil::MakeBinding_UBO(
            0, VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 1));

        mSpriteSetLayout1_Common = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (mSpriteSetLayout1_Common == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] setLayout1(SpriteCommon) create failed\n";
            return false;
        }
    }

    //========================================================
    // (2) 新規
    //========================================================
    auto pipeUP = std::make_unique<VKPipeline>();
    VKPipeline* pipe = pipeUP.get();

    pipe->debugName  = "Sprite";
    pipe->renderPass = mRenderPass;

    // ★共有参照（destroyしない）
    pipe->setLayout0 = mWorldSetLayout0_Texture;

    // push constants（サイズ一致が重要）
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset     = 0;
    pcr.size       = (uint32_t)sizeof(SpritePush); // 80

    // PipelineLayout: set0 + push
    {
        VkDescriptorSetLayout setLayouts[2] =
        {
            pipe->setLayout0,         // set=0 sampler
            mSpriteSetLayout1_Common  // set=1 sprite common
        };

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset     = 0;
        pcr.size       = (uint32_t)sizeof(SpritePush); // ★ 80 bytes

        VkPipelineLayoutCreateInfo lci{};
        lci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        lci.setLayoutCount         = 2;
        lci.pSetLayouts            = setLayouts;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges    = &pcr;

        if (vkCreatePipelineLayout(mDevice, &lci, nullptr, &pipe->pipelineLayout) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] CreateSpritePipeline: vkCreatePipelineLayout failed.\n";
            return false;
        }
    }

    //========================================================
    // (3) Shader modules
    //========================================================
    const std::string vsPath = "ToyLib/Shaders/VK/spv/Sprite.vert.spv";
    const std::string fsPath = "ToyLib/Shaders/VK/spv/Sprite.frag.spv";

    std::vector<uint8_t> vsCode, fsCode;
    if (!vkutil::ReadFileBinary(vsPath, vsCode))
    {
        std::cerr << "[VKRenderer] CreateSpritePipeline: failed to read VS: " << vsPath << "\n";
        return false;
    }
    if (!vkutil::ReadFileBinary(fsPath, fsCode))
    {
        std::cerr << "[VKRenderer] CreateSpritePipeline: failed to read FS: " << fsPath << "\n";
        return false;
    }

    VkShaderModule vs = vkutil::CreateShaderModule(mDevice, vsCode);
    VkShaderModule fs = vkutil::CreateShaderModule(mDevice, fsCode);
    if (!vs || !fs)
    {
        std::cerr << "[VKRenderer] CreateSpritePipeline: CreateShaderModule failed.\n";
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
    // (4) Vertex input
    //========================================================
    const VkVertexInputBindingDescription binding = MakeSpriteBinding();
    const auto attrs = MakeSpriteAttributes();

    VkPipelineVertexInputStateCreateInfo vis{};
    vis.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount   = 1;
    vis.pVertexBindingDescriptions      = &binding;
    vis.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    vis.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    //========================================================
    // (5) Dynamic viewport/scissor
    //========================================================
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkDynamicState dyns[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dyns;

    //========================================================
    // (6) Raster / MSAA / Depth / Blend（そのまま）
    //========================================================
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_NONE;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dss{};
    dss.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dss.depthTestEnable  = VK_FALSE;
    dss.depthWriteEnable = VK_FALSE;
    dss.depthCompareOp   = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState cbA{};
    cbA.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    cbA.blendEnable         = VK_TRUE;
    cbA.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbA.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbA.colorBlendOp        = VK_BLEND_OP_ADD;

    cbA.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbA.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbA.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbA;

    //========================================================
    // (7) Create pipeline
    //========================================================
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
        std::cerr << "[VKRenderer] vkCreateGraphicsPipelines(Sprite) failed: " << r << "\n";
        vkDestroyPipelineLayout(mDevice, pipe->pipelineLayout, nullptr);
        pipe->pipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    mPipelines["Sprite"] = std::move(pipeUP);
    return true;
}
} // namespace toy
