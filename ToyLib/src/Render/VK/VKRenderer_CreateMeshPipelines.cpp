// Render/VK/VKRenderer_CreateMeshPipelines.cpp（Mesh: Cull variants）

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include "Utils/MathUtil.h"
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

static VkCullModeFlags ToVkCullMode(CullMode cull)
{
    switch (cull)
    {
    case CullMode::None:  return VK_CULL_MODE_NONE;
    case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_BACK_BIT;
}

static VkFrontFace ToVkFrontFace(FrontFace ff)
{
    return (ff == FrontFace::CW) ? VK_FRONT_FACE_CLOCKWISE
                                 : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

bool VKRenderer::CreateMeshPipeline()
{
    //========================================================
    // 共有テクスチャ
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

    if (mWorldSetLayout1_Common == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> b;

        // binding 0 : WorldCommon (VS+FS)
        b.push_back(vkutil::MakeBinding_UBO(
            0, VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 1));

        // binding 1 : DirLight (FS)
        b.push_back(vkutil::MakeBinding_UBO(
            1, VK_SHADER_STAGE_FRAGMENT_BIT, 1));

        // binding 2 : PointLight (FS)
        b.push_back(vkutil::MakeBinding_UBO(
            2, VK_SHADER_STAGE_FRAGMENT_BIT, 1));

        mWorldSetLayout1_Common = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (mWorldSetLayout1_Common == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] setLayout1(Common) create failed\n";
            return false;
        }
    }
    
    //========================================================
    // Load SPIR-V once
    //========================================================
    std::vector<uint8_t> vertCode, fragCode;

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

    //========================================================
    // Common fixed states
    //========================================================
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

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp   = VK_COMPARE_OP_ALWAYS;

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

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    //========================================================
    // Local helper: make one variant
    //========================================================
    auto createVariant = [&](const char* name, CullMode cull, FrontFace ff) -> bool
    {
        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode    = ToVkCullMode(cull);
        rs.frontFace   = ToVkFrontFace(ff);
        rs.lineWidth   = 1.0f;

        // ★共有 set layout を使う（Descriptors.cpp と必ず一致させる）
        VkDescriptorSetLayout setLayouts[2] = {
            mWorldSetLayout0_Texture,
            mWorldSetLayout1_Common
        };

        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.size       = sizeof(PushConstants_Mesh);
        pc.offset     = 0;

        VkPipelineLayoutCreateInfo pl{};
        pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pl.setLayoutCount         = 2;
        pl.pSetLayouts            = setLayouts;
        pl.pushConstantRangeCount = 1;
        pl.pPushConstantRanges    = &pc;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(mDevice, &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            std::cerr << "Mesh pipeline layout failed: " << name << "\n";
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
            std::cerr << "Mesh pipeline create failed: " << name << "\n";
            vkDestroyPipelineLayout(mDevice, pipelineLayout, nullptr);
            return false;
        }

        auto p = std::make_unique<VKPipeline>();
        p->debugName      = name;
        p->pipeline       = pipeline;
        p->pipelineLayout = pipelineLayout;

        // ★共有参照（destroyしない）
        p->setLayout0     = mWorldSetLayout0_Texture;
        p->setLayout1     = mWorldSetLayout1_Common;

        p->renderPass     = mRenderPass;

        mPipelines[name] = std::move(p);
        return true;
    };

    //========================================================
    // Create variants (DrawWorldのキーと一致させる)
    //========================================================
    bool ok = true;

    // 旧コード互換：まず "Mesh" をデフォルトとして作る（Back + CCW）
    ok &= createVariant("Mesh", CullMode::Back, FrontFace::CCW);

    // 派生：ResolveWorldPipelineForItem が探すキーに合わせる
    ok &= createVariant("Mesh_CullNone_CCW",  CullMode::None,  FrontFace::CCW);
    ok &= createVariant("Mesh_CullBack_CCW",  CullMode::Back,  FrontFace::CCW);
    ok &= createVariant("Mesh_CullFront_CCW", CullMode::Front, FrontFace::CCW);

    // 必要なら CW 系も（今後の反転メッシュ/裏返し表現で使う）
    ok &= createVariant("Mesh_CullNone_CW",  CullMode::None,  FrontFace::CW);
    ok &= createVariant("Mesh_CullBack_CW",  CullMode::Back,  FrontFace::CW);
    ok &= createVariant("Mesh_CullFront_CW", CullMode::Front, FrontFace::CW);

    vkDestroyShaderModule(mDevice, vertModule, nullptr);
    vkDestroyShaderModule(mDevice, fragModule, nullptr);

    return ok;
}

} // namespace toy
