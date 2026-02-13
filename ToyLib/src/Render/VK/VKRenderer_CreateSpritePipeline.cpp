//==============================================================================
// VKRenderer_SpritePipeline.cpp
//  - CreateSpritePipeline() full implementation
//  - Sprite: 2D textured quad (pos+uv) + alpha blend
//  - Descriptor: set=0 binding=0 combined image sampler (uTex)
//  - PushConstants: MVP + color/alpha
//
// Assumptions:
//  - mDevice / mRenderPass / mSwapchainExtent are valid
//  - You have a VKPipeline wrapper class (or struct) to store:
//      VkPipeline pipeline;
//      VkPipelineLayout pipelineLayout;
//      VkDescriptorSetLayout setLayout;
//      VkRenderPass renderPass;
//  - You keep pipelines in: std::unordered_map<std::string, std::unique_ptr<VKPipeline>> mPipelines;
//  - SPIR-V files exist (example):
//      ToyLib/Shaders/VK/Sprite.vert.spv
//      ToyLib/Shaders/VK/Sprite.frag.spv
//==============================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <cstring>

namespace toy {

//------------------------------------------------------------------------------
// File read (SPIR-V)
//------------------------------------------------------------------------------
static bool ReadFileBinary(const char* path, std::vector<uint8_t>& out)
{
    out.clear();

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        std::cerr << "[VK] Failed to open file: " << path << "\n";
        return false;
    }

    const std::streamsize size = ifs.tellg();
    if (size <= 0)
    {
        std::cerr << "[VK] File empty: " << path << "\n";
        return false;
    }

    out.resize((size_t)size);
    ifs.seekg(0, std::ios::beg);

    if (!ifs.read((char*)out.data(), size))
    {
        std::cerr << "[VK] Failed to read file: " << path << "\n";
        out.clear();
        return false;
    }

    return true;
}

static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& bytes)
{
    if (!device || bytes.empty() || (bytes.size() % 4) != 0)
    {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = bytes.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(bytes.data());

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return mod;
}

//------------------------------------------------------------------------------
// Sprite vertex declaration (pos.xyz + uv.xy)
// NOTE:
//  - This must match your sprite quad vertex buffer layout.
//  - If your VertexArray uses different layout, adjust offsets/formats.
//------------------------------------------------------------------------------
struct VKSpriteVertex
{
    float px, py, pz;
    float u, v;
};

//------------------------------------------------------------------------------
// PushConstants (MVP + color/alpha)
// NOTE:
//  - Keep under device limit (often 128 bytes on many devices).
//  - Mat4(64) + vec4(16) = 80 bytes OK.
//------------------------------------------------------------------------------
struct SpritePushConstants
{
    float mvp[16];   // column-major or row-major: match shader expectation!
    float color[4];  // rgb + alpha
};

//==============================================================================
// CreateSpritePipeline
//==============================================================================
bool VKRenderer::CreateSpritePipeline()
{
    if (!mDevice || mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSpritePipeline failed: device/renderpass not ready\n";
        return false;
    }

    // If already exists, destroy and recreate (safe)
    {
        auto it = mPipelines.find("Sprite");
        if (it != mPipelines.end() && it->second)
        {
            auto* p = it->second.get();
            if (p->pipeline)       vkDestroyPipeline(mDevice, p->pipeline, nullptr);
            if (p->pipelineLayout) vkDestroyPipelineLayout(mDevice, p->pipelineLayout, nullptr);
            if (p->setLayout)      vkDestroyDescriptorSetLayout(mDevice, p->setLayout, nullptr);
            it->second.reset();
            mPipelines.erase(it);
        }
    }

    // ----------------------------------------------------------
    // 1) Shader modules
    // ----------------------------------------------------------
    // You can use your mShaderPath or a dedicated VK shader folder.
    // Example: "ToyLib/Shaders/VK/"
    const std::string base = mShaderPath + "VK/spv/";
    const std::string vsPath = base + "Sprite.vert.spv";
    const std::string fsPath = base + "Sprite.frag.spv";

    std::vector<uint8_t> vsBytes;
    std::vector<uint8_t> fsBytes;

    if (!ReadFileBinary(vsPath.c_str(), vsBytes) ||
        !ReadFileBinary(fsPath.c_str(), fsBytes))
    {
        std::cerr << "[VKRenderer] CreateSpritePipeline failed: missing SPIR-V\n";
        return false;
    }

    VkShaderModule vs = CreateShaderModule(mDevice, vsBytes);
    VkShaderModule fs = CreateShaderModule(mDevice, fsBytes);
    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
    {
        if (vs) vkDestroyShaderModule(mDevice, vs, nullptr);
        if (fs) vkDestroyShaderModule(mDevice, fs, nullptr);
        std::cerr << "[VKRenderer] CreateSpritePipeline failed: shader module create failed\n";
        return false;
    }

    VkPipelineShaderStageCreateInfo stageVS{};
    stageVS.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageVS.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stageVS.module = vs;
    stageVS.pName  = "main";

    VkPipelineShaderStageCreateInfo stageFS{};
    stageFS.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageFS.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stageFS.module = fs;
    stageFS.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[] = { stageVS, stageFS };

    // ----------------------------------------------------------
    // 2) DescriptorSetLayout (set=0 binding=0: combined image sampler)
    // ----------------------------------------------------------
    VkDescriptorSetLayoutBinding texBinding{};
    texBinding.binding            = 0;
    texBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texBinding.descriptorCount    = 1;
    texBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    texBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo slci{};
    slci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    slci.bindingCount = 1;
    slci.pBindings    = &texBinding;

    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(mDevice, &slci, nullptr, &setLayout) != VK_SUCCESS)
    {
        vkDestroyShaderModule(mDevice, vs, nullptr);
        vkDestroyShaderModule(mDevice, fs, nullptr);
        std::cerr << "[VKRenderer] CreateSpritePipeline failed: vkCreateDescriptorSetLayout\n";
        return false;
    }

    // ----------------------------------------------------------
    // 3) PipelineLayout (Descriptor + PushConstants)
    // ----------------------------------------------------------
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset     = 0;
    pcr.size       = sizeof(SpritePushConstants);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &pcr;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(mDevice, &plci, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        vkDestroyDescriptorSetLayout(mDevice, setLayout, nullptr);
        vkDestroyShaderModule(mDevice, vs, nullptr);
        vkDestroyShaderModule(mDevice, fs, nullptr);
        std::cerr << "[VKRenderer] CreateSpritePipeline failed: vkCreatePipelineLayout\n";
        return false;
    }

    // ----------------------------------------------------------
    // 4) Vertex Input (pos3 + uv2)
    // ----------------------------------------------------------
    VkVertexInputBindingDescription bind{};
    bind.binding   = 0;
    bind.stride    = sizeof(VKSpriteVertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrPos{};
    attrPos.location = 0;
    attrPos.binding  = 0;
    attrPos.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrPos.offset   = offsetof(VKSpriteVertex, px);

    VkVertexInputAttributeDescription attrUV{};
    attrUV.location = 1;
    attrUV.binding  = 0;
    attrUV.format   = VK_FORMAT_R32G32_SFLOAT;
    attrUV.offset   = offsetof(VKSpriteVertex, u);

    std::array<VkVertexInputAttributeDescription, 2> attrs = { attrPos, attrUV };

    VkPipelineVertexInputStateCreateInfo vis{};
    vis.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount   = 1;
    vis.pVertexBindingDescriptions      = &bind;
    vis.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    vis.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    // ----------------------------------------------------------
    // 5) Viewport/Scissor (use dynamic)
    // ----------------------------------------------------------
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineDynamicStateCreateInfo dyn{};
    std::array<VkDynamicState, 2> dynStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = (uint32_t)dynStates.size();
    dyn.pDynamicStates    = dynStates.data();

    // ----------------------------------------------------------
    // 6) Rasterizer
    // NOTE: UI spriteなら cull none が安定（RenderItem.cull をVKで反映するなら別pipelineが必要）
    // ----------------------------------------------------------
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = VK_CULL_MODE_NONE; // Sprite is usually double-sided
    rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable         = VK_FALSE;
    rs.lineWidth               = 1.0f;

    // ----------------------------------------------------------
    // 7) Multisample
    // ----------------------------------------------------------
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ----------------------------------------------------------
    // 8) Depth/Stencil (UI: disabled)
    // ----------------------------------------------------------
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;

    // ----------------------------------------------------------
    // 9) Color blend (alpha)
    // ----------------------------------------------------------
    VkPipelineColorBlendAttachmentState att{};
    att.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    att.blendEnable         = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp        = VK_BLEND_OP_ADD;

    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.logicOpEnable   = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments    = &att;

    // ----------------------------------------------------------
    // 10) Graphics Pipeline create
    // ----------------------------------------------------------
    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount          = 2;
    gpci.pStages             = stages;

    gpci.pVertexInputState   = &vis;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState      = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState   = &ms;
    gpci.pDepthStencilState  = &ds;
    gpci.pColorBlendState    = &cb;
    gpci.pDynamicState       = &dyn;

    gpci.layout              = pipelineLayout;
    gpci.renderPass          = mRenderPass;
    gpci.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pr = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline);

    // Shader modules can be destroyed after pipeline creation
    vkDestroyShaderModule(mDevice, vs, nullptr);
    vkDestroyShaderModule(mDevice, fs, nullptr);

    if (pr != VK_SUCCESS || pipeline == VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(mDevice, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, setLayout, nullptr);
        std::cerr << "[VKRenderer] CreateSpritePipeline failed: vkCreateGraphicsPipelines: " << pr << "\n";
        return false;
    }

    // ----------------------------------------------------------
    // 11) Store into map
    // ----------------------------------------------------------
    auto p = std::make_unique<VKPipeline>();
    p->pipeline       = pipeline;
    p->pipelineLayout = pipelineLayout;
    p->setLayout      = setLayout;
    p->renderPass     = mRenderPass;

    mPipelines["Sprite"] = std::move(p);

    std::cerr << "[VKRenderer] Sprite pipeline created.\n";
    return true;
}

} // namespace toy
