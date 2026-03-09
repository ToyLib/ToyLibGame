#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/VKUtil.h" // ReadFileBinary / CreateShaderModule
#include <iostream>
#include <vector>
#include <cstring>

namespace toy
{

VKPipeline::~VKPipeline()
{
    Destroy();
}

void VKPipeline::Destroy()
{
    if (!mDevice) return;

    // NOTE:
    // 破棄順は「Pipeline -> PipelineLayout -> DescriptorSetLayouts」が安全。

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

    if (!mSetLayouts.empty())
    {
        for (auto& sl : mSetLayouts)
        {
            if (sl)
            {
                vkDestroyDescriptorSetLayout(mDevice, sl, nullptr);
                sl = VK_NULL_HANDLE;
            }
        }
        mSetLayouts.clear();
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

void VKPipeline::BuildVertexInput(
    VKPipelineDesc::VertexLayout layout,
    std::vector<VkVertexInputBindingDescription>& outBindings,
    std::vector<VkVertexInputAttributeDescription>& outAttrs)
{
    outBindings.clear();
    outAttrs.clear();

    auto addBinding = [&](uint32_t binding, uint32_t stride, VkVertexInputRate rate)
    {
        VkVertexInputBindingDescription b{};
        b.binding   = binding;
        b.stride    = stride;
        b.inputRate = rate;
        outBindings.push_back(b);
    };

    auto addAttr = [&](uint32_t loc, uint32_t binding, VkFormat fmt, uint32_t offset)
    {
        VkVertexInputAttributeDescription a{};
        a.location = loc;
        a.binding  = binding;
        a.format   = fmt;
        a.offset   = offset;
        outAttrs.push_back(a);
    };

    switch (layout)
    {
        case VKPipelineDesc::VertexLayout::Sprite_Pos3Nrm3Uv2:
        case VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2:
        {
            // binding 0 : pos3 + nrm3 + uv2
            addBinding(0, 32, VK_VERTEX_INPUT_RATE_VERTEX);

            addAttr(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
            addAttr(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);
            addAttr(2, 0, VK_FORMAT_R32G32_SFLOAT,    24);
            break;
        }

        case VKPipelineDesc::VertexLayout::Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4:
        {
            // binding 0 : pos3 + nrm3 + uv2 + bone4u32 + weight4
            addBinding(0, 64, VK_VERTEX_INPUT_RATE_VERTEX);

            addAttr(0, 0, VK_FORMAT_R32G32B32_SFLOAT,     0);
            addAttr(1, 0, VK_FORMAT_R32G32B32_SFLOAT,    12);
            addAttr(2, 0, VK_FORMAT_R32G32_SFLOAT,       24);
            addAttr(3, 0, VK_FORMAT_R32G32B32A32_UINT,   32);
            addAttr(4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 48);
            break;
        }

        case VKPipelineDesc::VertexLayout::Vec2_Pos2:
        {
            // binding 0 : pos2
            addBinding(0, 8, VK_VERTEX_INPUT_RATE_VERTEX);

            addAttr(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
            break;
        }
            
        case VKPipelineDesc::VertexLayout::Particle_Pos3Uv2_InstPos3Life1:
        {
            addBinding(0, 32, VK_VERTEX_INPUT_RATE_VERTEX);
            addAttr(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
            addAttr(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);
            addAttr(2, 0, VK_FORMAT_R32G32_SFLOAT,    24);

            addBinding(1, 32, VK_VERTEX_INPUT_RATE_INSTANCE);
            addAttr(3, 1, VK_FORMAT_R32G32B32_SFLOAT, 0);   // iPos
            addAttr(4, 1, VK_FORMAT_R32_SFLOAT,       12);  // iLife
            break;
        }
        default:
        {
            // fallback : Mesh_Pos3Nrm3Uv2 相当
            addBinding(0, 32, VK_VERTEX_INPUT_RATE_VERTEX);

            addAttr(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
            addAttr(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);
            addAttr(2, 0, VK_FORMAT_R32G32_SFLOAT,    24);
            break;
        }
    }
}

//======================================================================
// CreateDescriptorSetLayouts
// 重要:
//  - set=2 だけを使う pipeline（set=1 が無い等）が存在すると、
//    [0]=valid [1]=NULL [2]=valid みたいな “穴あき配列” ができる。
//  - それを vkCreatePipelineLayout に渡すのは未定義寄りで、MVK等でクラッシュ要因。
//  -> maxSet までの “穴” は bindingCount=0 の空レイアウトで埋める。
//======================================================================
bool VKPipeline::CreateDescriptorSetLayouts(const VKPipelineDesc& desc)
{
    // set layout を使わない pipeline もある
    if (desc.setLayouts.empty())
    {
        // 既存を破棄
        if (!mSetLayouts.empty() && mDevice)
        {
            for (auto& sl : mSetLayouts)
            {
                if (sl)
                {
                    vkDestroyDescriptorSetLayout(mDevice, sl, nullptr);
                    sl = VK_NULL_HANDLE;
                }
            }
        }
        mSetLayouts.clear();
        return true;
    }

    if (!mDevice)
    {
        std::cerr << "[VKPipeline] CreateDescriptorSetLayouts: mDevice is null\n";
        return false;
    }

    uint32_t maxSet = 0;
    for (const auto& s : desc.setLayouts)
    {
        if (s.set > maxSet) maxSet = s.set;
    }

    // 既存を破棄
    if (!mSetLayouts.empty())
    {
        for (auto& sl : mSetLayouts)
        {
            if (sl)
            {
                vkDestroyDescriptorSetLayout(mDevice, sl, nullptr);
                sl = VK_NULL_HANDLE;
            }
        }
        mSetLayouts.clear();
    }

    // maxSet まで確保（穴は後で埋める）
    mSetLayouts.assign(maxSet + 1, VK_NULL_HANDLE);

    auto cleanupOnFail = [&]()
    {
        for (auto& sl : mSetLayouts)
        {
            if (sl)
            {
                vkDestroyDescriptorSetLayout(mDevice, sl, nullptr);
                sl = VK_NULL_HANDLE;
            }
        }
        mSetLayouts.clear();
    };

    // まず指定された set だけ作る
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
            cleanupOnFail();
            return false;
        }

        mSetLayouts[s.set] = out;
    }

    // ★穴埋め：未指定 set は “空レイアウト” を作る
    for (uint32_t si = 0; si < static_cast<uint32_t>(mSetLayouts.size()); ++si)
    {
        if (mSetLayouts[si] != VK_NULL_HANDLE)
        {
            continue;
        }

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 0;
        ci.pBindings    = nullptr;

        VkDescriptorSetLayout empty = VK_NULL_HANDLE;
        VkResult vr = vkCreateDescriptorSetLayout(mDevice, &ci, nullptr, &empty);
        if (vr != VK_SUCCESS || !empty)
        {
            std::cerr << "[VKPipeline] vkCreateDescriptorSetLayout(empty) failed: " << vr
                      << " (set=" << si << ")\n";
            cleanupOnFail();
            return false;
        }

        mSetLayouts[si] = empty;
    }

    return true;
}

bool VKPipeline::Create(VkDevice device,
                        VkRenderPass renderPass,
                        VkExtent2D extent,
                        const VKPipelineDesc& desc)
{
    Destroy();

    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] Create failed: invalid device/renderPass\n";
        return false;
    }

    if (extent.width == 0 || extent.height == 0)
    {
        std::cerr << "[VKPipeline] Create failed: invalid extent\n";
        return false;
    }

    if (desc.vsPath.empty() || desc.fsPath.empty())
    {
        std::cerr << "[VKPipeline] Create failed: shader path is empty\n";
        return false;
    }

    mDevice = device;

    //----------------------------------------------------------
    // Shader modules
    //----------------------------------------------------------
    VkShaderModule vs = LoadShaderModule(device, desc.vsPath);
    if (vs == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] LoadShaderModule failed: " << desc.vsPath << "\n";
        return false;
    }

    VkShaderModule fs = LoadShaderModule(device, desc.fsPath);
    if (fs == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPipeline] LoadShaderModule failed: " << desc.fsPath << "\n";
        vkDestroyShaderModule(device, vs, nullptr);
        return false;
    }

    //----------------------------------------------------------
    // Shader stages
    //----------------------------------------------------------
    VkPipelineShaderStageCreateInfo stages[2]{};

    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName  = "main";

    //----------------------------------------------------------
    // Vertex input
    //----------------------------------------------------------
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    BuildVertexInput(desc.layout, bindings, attrs);    
    
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
    vi.pVertexBindingDescriptions      = bindings.empty() ? nullptr : bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions    = attrs.empty() ? nullptr : attrs.data();

    //----------------------------------------------------------
    // Input assembly
    //----------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology               = desc.topology;
    ia.primitiveRestartEnable = VK_FALSE;

    //----------------------------------------------------------
    // Viewport / Scissor
    //----------------------------------------------------------
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

    //----------------------------------------------------------
    // Rasterizer
    //----------------------------------------------------------
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = desc.cullMode;
    rs.frontFace               = desc.frontFace;
    rs.depthBiasEnable         = desc.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rs.depthBiasConstantFactor = desc.depthBiasConstantFactor;
    rs.depthBiasClamp          = desc.depthBiasClamp;
    rs.depthBiasSlopeFactor    = desc.depthBiasSlopeFactor;
    rs.lineWidth               = 1.0f;

    //----------------------------------------------------------
    // Multisample
    //----------------------------------------------------------
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ms.sampleShadingEnable  = VK_FALSE;

    //----------------------------------------------------------
    // Depth stencil
    //----------------------------------------------------------
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable       = desc.depthTest  ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable      = desc.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable     = VK_FALSE;

    //----------------------------------------------------------
    // Color blend
    //----------------------------------------------------------
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    blendAttachments.resize(desc.colorAttachmentCount);

    for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i)
    {
        auto& a = blendAttachments[i];
        a.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        switch (desc.blendMode)
        {
            case VKPipelineDesc::BlendMode::Opaque:
            {
                a.blendEnable = VK_FALSE;
                break;
            }

            case VKPipelineDesc::BlendMode::Alpha:
            {
                a.blendEnable         = VK_TRUE;
                a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                a.colorBlendOp        = VK_BLEND_OP_ADD;
                a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                a.alphaBlendOp        = VK_BLEND_OP_ADD;
                break;
            }

            case VKPipelineDesc::BlendMode::Additive:
            {
                a.blendEnable         = VK_TRUE;
                a.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                a.colorBlendOp        = VK_BLEND_OP_ADD;
                a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                a.alphaBlendOp        = VK_BLEND_OP_ADD;
                break;
            }
        }
    }

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.logicOpEnable   = VK_FALSE;
    cb.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    cb.pAttachments    = blendAttachments.empty() ? nullptr : blendAttachments.data();

    //----------------------------------------------------------
    // Descriptor set layouts
    //----------------------------------------------------------
    std::vector<VkDescriptorSetLayout> setLayouts;
    mSetLayouts.clear();

    if (!desc.setLayouts.empty())
    {
        uint32_t maxSet = 0;
        for (const auto& sl : desc.setLayouts)
        {
            maxSet = std::max(maxSet, sl.set);
        }

        mSetLayouts.resize(maxSet + 1, VK_NULL_HANDLE);
        setLayouts.resize(maxSet + 1, VK_NULL_HANDLE);

        for (const auto& sl : desc.setLayouts)
        {
            std::vector<VkDescriptorSetLayoutBinding> bindingsDesc;
            bindingsDesc.reserve(sl.bindings.size());

            for (const auto& b : sl.bindings)
            {
                VkDescriptorSetLayoutBinding vkbind{};
                vkbind.binding            = b.binding;
                vkbind.descriptorType     = b.type;
                vkbind.descriptorCount    = b.count;
                vkbind.stageFlags         = b.stages;
                vkbind.pImmutableSamplers = nullptr;
                bindingsDesc.push_back(vkbind);
            }

            VkDescriptorSetLayoutCreateInfo linfo{};
            linfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            linfo.bindingCount = static_cast<uint32_t>(bindingsDesc.size());
            linfo.pBindings    = bindingsDesc.empty() ? nullptr : bindingsDesc.data();

            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            VkResult lres = vkCreateDescriptorSetLayout(device, &linfo, nullptr, &layout);
            if (lres != VK_SUCCESS)
            {
                std::cerr << "[VKPipeline] vkCreateDescriptorSetLayout failed: " << lres << "\n";
                vkDestroyShaderModule(device, vs, nullptr);
                vkDestroyShaderModule(device, fs, nullptr);
                Destroy();
                return false;
            }

            mSetLayouts[sl.set] = layout;
            setLayouts[sl.set]  = layout;
        }
    }

    //----------------------------------------------------------
    // Push constant ranges
    //----------------------------------------------------------
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

    //----------------------------------------------------------
    // Pipeline layout
    //----------------------------------------------------------
    VkPipelineLayoutCreateInfo pl{};
    pl.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    pl.pSetLayouts            = setLayouts.empty() ? nullptr : setLayouts.data();
    pl.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size());
    pl.pPushConstantRanges    = pcRanges.empty() ? nullptr : pcRanges.data();

    if (vkCreatePipelineLayout(device, &pl, nullptr, &mLayout) != VK_SUCCESS)
    {
        std::cerr << "[VKPipeline] vkCreatePipelineLayout failed\n";
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, fs, nullptr);
        Destroy();
        return false;
    }

    //----------------------------------------------------------
    // Graphics pipeline
    //----------------------------------------------------------
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
    gp.layout              = mLayout;
    gp.renderPass          = renderPass;
    gp.subpass             = 0;

    VkResult vr = vkCreateGraphicsPipelines(device,
                                            VK_NULL_HANDLE,
                                            1,
                                            &gp,
                                            nullptr,
                                            &mPipeline);

    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, fs, nullptr);

    if (vr != VK_SUCCESS)
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
