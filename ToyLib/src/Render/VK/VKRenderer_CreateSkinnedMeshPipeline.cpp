//======================================================================
// VKRenderer_CreateSkinnedMeshPipeline.cpp
//  - World SkinnedMesh 用パイプライン作成
//  - set=0 : World material (combined image samplers etc)  ※既存Meshと共通
//  - set=1 : World common + matrix palette (UBO)
//      binding=0 : WorldCommon (view/proj, light, etc)      ※既存Meshと同等
//      binding=4 : MatrixPalette (mat4[96])
//  - push constants : mat4 uWorldTransform (64 bytes)
//  - vertex layout : pos3 + normal3 + uv2 + bone(uvec4) + weight(vec4)
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"

#include <iostream>
#include <vector>
#include <array>

namespace toy {

//--------------------------------------------------------------
// Vertex input (Skinned)
//  - location=0 vec3 pos
//  - location=1 vec3 normal
//  - location=2 vec2 uv
//  - location=3 uvec4 boneIds
//  - location=4 vec4 weights
//
// ストライドは「あなたのSkinned用VertexArrayの実体」に合わせる必要がある。
// まずは float基準で組むと事故るので、ここでは明示で offsets を置く。
// 例）pos(12) normal(12) uv(8) bone(16) weight(16) = 64 bytes
//--------------------------------------------------------------
static VkVertexInputBindingDescription MakeSkinnedBinding()
{
    VkVertexInputBindingDescription b{};
    b.binding   = 0;
    b.stride    = 64; // ★まずはこの想定。実データと違うならここを合わせる。
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return b;
}

static std::array<VkVertexInputAttributeDescription, 5> MakeSkinnedAttributes()
{
    std::array<VkVertexInputAttributeDescription, 5> a{};

    // location=0 pos (vec3)
    a[0].location = 0;
    a[0].binding  = 0;
    a[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    a[0].offset   = 0;

    // location=1 normal (vec3)
    a[1].location = 1;
    a[1].binding  = 0;
    a[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    a[1].offset   = 12;

    // location=2 uv (vec2)
    a[2].location = 2;
    a[2].binding  = 0;
    a[2].format   = VK_FORMAT_R32G32_SFLOAT;
    a[2].offset   = 24;

    // location=3 boneIds (uvec4)
    //  ここは「uint32*4」が安全。VK_FORMAT_R32G32B32A32_UINT
    a[3].location = 3;
    a[3].binding  = 0;
    a[3].format   = VK_FORMAT_R32G32B32A32_UINT;
    a[3].offset   = 32;

    // location=4 weights (vec4)
    a[4].location = 4;
    a[4].binding  = 0;
    a[4].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    a[4].offset   = 48;

    return a;
}

//--------------------------------------------------------------
// VKRenderer::CreateSkinnedMeshPipeline
//--------------------------------------------------------------
bool VKRenderer::CreateSkinnedMeshPipeline()
{
    if (!mDevice || !mRenderPass)
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: device/renderPass not ready.\n";
        return false;
    }

    //========================================================
    // (0) set=0 (World material) は Mesh と共通の想定
    //========================================================
    // 既に Mesh pipeline 作成時に mWorldSetLayout0_Material を作っているならそれを使う。
    if (mWorldSetLayout0_Texture == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSkinnedMeshPipeline: mWorldSetLayout0_Material is null. Create Mesh pipeline first.\n";
        return false;
    }

    //========================================================
    // (1) set=1 Skinned 用 (WorldCommon + Palette) を用意
    //========================================================
    // 既存 Mesh が set=1 に WorldCommon を持っているなら、Skinned は “追加で binding=4” を入れた別layoutにする（最短）
    if (mWorldSetLayout1_Skinned == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> b;

        // binding=0 : WorldCommon UBO（Meshと共通）
        b.push_back(vkutil::MakeBinding_UBO(
            0,
            VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
            1));

        // binding=4 : MatrixPalette UBO（頂点シェーダ専用でOK）
        b.push_back(vkutil::MakeBinding_UBO(
            4,
            VK_SHADER_STAGE_VERTEX_BIT,
            1));

        mWorldSetLayout1_Skinned = vkutil::CreateDescriptorSetLayout(mDevice, b);
        if (mWorldSetLayout1_Skinned == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] setLayout1(Skinned) create failed\n";
            return false;
        }
    }

    //========================================================
    // (2) 既存の SkinnedMesh pipeline があれば破棄
    //========================================================
    auto itOld = mPipelines.find("SkinnedMesh");
    if (itOld != mPipelines.end() && itOld->second)
    {
        VKPipeline* old = itOld->second.get();
        if (old->pipeline)       vkDestroyPipeline(mDevice, old->pipeline, nullptr);
        if (old->pipelineLayout) vkDestroyPipelineLayout(mDevice, old->pipelineLayout, nullptr);
        old->pipeline = VK_NULL_HANDLE;
        old->pipelineLayout = VK_NULL_HANDLE;
        old->setLayout0 = VK_NULL_HANDLE;
        old->setLayout1 = VK_NULL_HANDLE;
        mPipelines.erase(itOld);
    }

    //========================================================
    // (3) pipeline object
    //========================================================
    auto pipeUP = std::make_unique<VKPipeline>();
    VKPipeline* pipe = pipeUP.get();
    pipe->debugName  = "SkinnedMesh";
    pipe->renderPass = mRenderPass;

    pipe->setLayout0 = mWorldSetLayout0_Texture;
    pipe->setLayout1 = mWorldSetLayout1_Skinned;

    //========================================================
    // (4) pipeline layout (set0 + set1 + push(world))
    //========================================================
    {
        VkDescriptorSetLayout setLayouts[2] = { pipe->setLayout0, pipe->setLayout1 };

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcr.offset     = 0;
        pcr.size       = (uint32_t)sizeof(Matrix4); // world only (64)

        VkPipelineLayoutCreateInfo lci{};
        lci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        lci.setLayoutCount         = 2;
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
    // (5) shaders
    //========================================================
    // 例：Skinned.vert + Mesh.frag（GLと同じ分け方）
    const std::string vsPath = "ToyLib/Shaders/VK/spv/Skinned.vert.spv";
    const std::string fsPath = "ToyLib/Shaders/VK/spv/Mesh.frag.spv";

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
    // (6) vertex input
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

    //========================================================
    // (7) viewport/scissor (dynamic)
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
    // (8) raster / msaa / depth / blend (Meshと同等にする)
    //========================================================
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

    //========================================================
    // (9) create pipeline
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
        std::cerr << "[VKRenderer] vkCreateGraphicsPipelines(SkinnedMesh) failed: " << r << "\n";
        vkDestroyPipelineLayout(mDevice, pipe->pipelineLayout, nullptr);
        pipe->pipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    mPipelines["SkinnedMesh"] = std::move(pipeUP);
    return true;
}

} // namespace toy
