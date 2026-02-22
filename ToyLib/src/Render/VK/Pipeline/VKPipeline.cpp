// Render/VK/VKPipeline.cpp
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/VKUtil.h" // ReadFileBinary / CreateShaderModule
#include <iostream>

namespace toy
{

VKPipeline::~VKPipeline()
{
    Destroy();
}

void VKPipeline::Destroy()
{
    if (!mDevice) return;

    if (!mSetLayouts.empty())
    {
        for (auto& sl : mSetLayouts)
        {
            if (sl)
            {
                vkDestroyDescriptorSetLayout(mDevice, sl, nullptr);
            }
        }
        mSetLayouts.clear();
    }
    
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

VkShaderModule VKPipeline::LoadShaderModule(VkDevice device, const std::string& spvPath)
{
    std::vector<uint8_t> bytes;
    if (!toy::vkutil::ReadFileBinary(spvPath, bytes))
    {
        std::cerr << "[VKPipeline] Failed to read spv: " << spvPath << "\n";
        return VK_NULL_HANDLE;
    }

    VkShaderModule mod = toy::vkutil::CreateShaderModule(device, bytes);
    if (!mod)
    {
        std::cerr << "[VKPipeline] CreateShaderModule failed: " << spvPath << "\n";
    }
    return mod;
}

void VKPipeline::BuildVertexInput(VKPipelineDesc::VertexLayout layout,
                                  VkVertexInputBindingDescription& outBinding,
                                  std::vector<VkVertexInputAttributeDescription>& outAttrs)
{
    outAttrs.clear();

    outBinding = {};
    outBinding.binding   = 0;
    outBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    auto pushAttr = [&](uint32_t loc, VkFormat fmt, uint32_t offset)
    {
        VkVertexInputAttributeDescription a{};
        a.location = loc;
        a.binding  = 0;
        a.format   = fmt;
        a.offset   = offset;
        outAttrs.push_back(a);
    };

    switch (layout)
    {
        case VKPipelineDesc::VertexLayout::Sprite_Pos3Nrm3Uv2:
        case VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2:
        {
            // interleaved: pos3(0) nrm3(12) uv2(24)
            outBinding.stride = 8 * sizeof(float); // 32
            pushAttr(0, VK_FORMAT_R32G32B32_SFLOAT, 0);
            pushAttr(1, VK_FORMAT_R32G32B32_SFLOAT, 12);
            pushAttr(2, VK_FORMAT_R32G32_SFLOAT,    24);
            break;
        }

        case VKPipelineDesc::VertexLayout::Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4:
        {
            // struct:
            // pos3(0) nrm3(12) uv2(24) bone4u32(32) weight4(48)
            outBinding.stride = 64;
            pushAttr(0, VK_FORMAT_R32G32B32_SFLOAT, 0);
            pushAttr(1, VK_FORMAT_R32G32B32_SFLOAT, 12);
            pushAttr(2, VK_FORMAT_R32G32_SFLOAT,    24);
            pushAttr(3, VK_FORMAT_R32G32B32A32_UINT, 32);
            pushAttr(4, VK_FORMAT_R32G32B32A32_SFLOAT, 48);
            break;
        }

        case VKPipelineDesc::VertexLayout::Vec2_Pos2:
        {
            // interleaved: pos2(0)
            outBinding.stride = 2 * sizeof(float); // 8
            pushAttr(0, VK_FORMAT_R32G32_SFLOAT, 0);
            break;
        }
    }
}

bool VKPipeline::CreateDescriptorSetLayouts(const VKPipelineDesc& desc)
{
    if (desc.setLayouts.empty())
    {
        mSetLayouts.clear();
        return true;
    }

    uint32_t maxSet = 0;
    for (const auto& s : desc.setLayouts)
    {
        if (s.set > maxSet) maxSet = s.set;
    }

    mSetLayouts.assign(maxSet + 1, VK_NULL_HANDLE);

    for (const auto& s : desc.setLayouts)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(s.bindings.size());

        for (const auto& b : s.bindings)
        {
            VkDescriptorSetLayoutBinding lb{};
            lb.binding            = b.binding;
            lb.descriptorType     = b.type;
            lb.descriptorCount    = b.count;
            lb.stageFlags         = b.stages;
            lb.pImmutableSamplers = nullptr;
            bindings.push_back(lb);
        }

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.empty() ? nullptr : bindings.data();

        VkDescriptorSetLayout out = VK_NULL_HANDLE;
        VkResult vr = vkCreateDescriptorSetLayout(mDevice, &ci, nullptr, &out);
        if (vr != VK_SUCCESS || !out)
        {
            std::cerr << "[VKPipeline] vkCreateDescriptorSetLayout failed: " << vr
                      << " (set=" << s.set << ")\n";
            return false;
        }

        mSetLayouts[s.set] = out;
    }

    return true;
}

bool VKPipeline::Create(VkDevice device,
                        VkRenderPass renderPass,
                        VkExtent2D extent,
                        const VKPipelineDesc& desc)
{
    Destroy();

    if (!device || !renderPass || extent.width == 0 || extent.height == 0)
    {
        return false;
    }
    if (desc.vsPath.empty() || desc.fsPath.empty())
    {
        std::cerr << "[VKPipeline] Create failed: empty shader path\n";
        return false;
    }

    mDevice = device;

    VkShaderModule vs = LoadShaderModule(device, desc.vsPath);
    VkShaderModule fs = LoadShaderModule(device, desc.fsPath);
    if (!vs || !fs)
    {
        if (vs) vkDestroyShaderModule(device, vs, nullptr);
        if (fs) vkDestroyShaderModule(device, fs, nullptr);
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

    // Vertex input
    VkVertexInputBindingDescription binding{};
    std::vector<VkVertexInputAttributeDescription> attrs;
    BuildVertexInput(desc.layout, binding, attrs);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport/scissor (you already set them per-frame)
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkDynamicState dynStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.lineWidth               = 1.0f;
    rs.cullMode                = desc.cullMode;
    rs.frontFace               = desc.frontFace;
    rs.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable        = desc.depthTest  ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable       = desc.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp         = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable  = VK_FALSE;
    ds.stencilTestEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    if (desc.alphaBlend)
    {
        cbAtt.blendEnable         = VK_TRUE;
        cbAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbAtt.colorBlendOp        = VK_BLEND_OP_ADD;
        cbAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbAtt.alphaBlendOp        = VK_BLEND_OP_ADD;
    }
    else
    {
        cbAtt.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAtt;

    // Pipeline layout（Desc 駆動：setLayouts）
    if (!CreateDescriptorSetLayouts(desc))
    {
        std::cerr << "[VKPipeline] CreateDescriptorSetLayouts failed\n";
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, fs, nullptr);
        Destroy();
        return false;
    }

    // ------------------------------------------------------------
    // Step2: Push constants（Desc 駆動）
    // ------------------------------------------------------------
    std::vector<VkPushConstantRange> pcRanges;
    pcRanges.reserve(desc.pushConstants.size());

    for (const auto& pc : desc.pushConstants)
    {
        VkPushConstantRange r{};
        r.stageFlags = pc.stages;
        r.offset     = pc.offset;
        r.size       = pc.size;
        pcRanges.push_back(r);
    }

    VkPipelineLayoutCreateInfo lci{};
    lci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lci.setLayoutCount = static_cast<uint32_t>(mSetLayouts.size());
    lci.pSetLayouts    = mSetLayouts.empty() ? nullptr : mSetLayouts.data();

    lci.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size());
    lci.pPushConstantRanges    = pcRanges.empty() ? nullptr : pcRanges.data();

    VkResult vr = vkCreatePipelineLayout(device, &lci, nullptr, &mLayout);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKPipeline] vkCreatePipelineLayout failed: " << vr << "\n";
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, fs, nullptr);
        Destroy();
        return false;
    }

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = mLayout;
    pci.renderPass          = renderPass;
    pci.subpass             = 0;

    vr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &mPipeline);

    // shader modules no longer needed after pipeline creation
    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, fs, nullptr);

    if (vr != VK_SUCCESS || !mPipeline)
    {
        std::cerr << "[VKPipeline] vkCreateGraphicsPipelines failed: " << vr << "\n";
        Destroy();
        return false;
    }

    return true;
}

void VKPipeline::Bind(VkCommandBuffer cmd) const
{
    if (!cmd || !mPipeline) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
}

} // namespace toy
