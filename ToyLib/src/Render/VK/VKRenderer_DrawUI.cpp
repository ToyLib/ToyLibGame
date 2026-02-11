//======================================================================
// VKRenderer_DrawUI.cpp
//  - DrawUIPass() 最小実装（テストスプライト表示まで）
//  - BeginFrame() が vkCmdBeginRenderPass 済み、EndFrame() が vkCmdEndRenderPass する前提
//  - RenderPass は swapchain color 1枚（depthなし）でOK
//
// 依存:
//  - VKRenderer.h に “UI用メンバと関数宣言” を少し追加（この下の「VKRenderer.h 追記」参照）
//  - シェーダーSPIR-Vを読み込む：
//      ToyLib/Shaders/VK/UI_Sprite.vert.spv
//      ToyLib/Shaders/VK/UI_Sprite.frag.spv
//
// GLSL例（別ファイルで glslc で .spv 生成）
//   UI_Sprite.vert
//     layout(location=0) in vec2 aPos;
//     layout(location=1) in vec2 aUV;
//     layout(push_constant) uniform PC { mat4 uMVP; vec4 uColor; } pc;
//     layout(location=0) out vec2 vUV;
//     layout(location=1) out vec4 vColor;
//     void main(){ vUV=aUV; vColor=pc.uColor; gl_Position = pc.uMVP * vec4(aPos,0,1); }
//
//   UI_Sprite.frag
//     layout(set=0,binding=0) uniform sampler2D uTex;
//     layout(location=0) in vec2 vUV;
//     layout(location=1) in vec4 vColor;
//     layout(location=0) out vec4 oColor;
//     void main(){ oColor = texture(uTex, vUV) * vColor; }
//
//======================================================================

#include "Render/VK/VKRenderer.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <cassert>

namespace toy {

//--------------------------------------------------------------
// ファイル読み込み（SPIR-V）
//--------------------------------------------------------------
static bool ReadFileBinary(const std::string& path, std::vector<uint8_t>& out)
{
    out.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        return false;
    }

    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    if (size <= 0)
    {
        return false;
    }
    f.seekg(0, std::ios::beg);

    out.resize((size_t)size);
    f.read((char*)out.data(), size);
    return true;
}

static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& code)
{
    if (code.empty() || (code.size() % 4) != 0)
    {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = (uint32_t)code.size();
    ci.pCode    = (const uint32_t*)code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return mod;
}

//--------------------------------------------------------------
// メモリタイプ探索
//--------------------------------------------------------------
static uint32_t FindMemoryType(VkPhysicalDevice phys,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags required)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        const bool typeOK = (typeBits & (1u << i)) != 0;
        const bool propOK = (memProps.memoryTypes[i].propertyFlags & required) == required;
        if (typeOK && propOK)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

//--------------------------------------------------------------
// バッファ作成（HostVisibleで雑に）
//  ※最短で表示したいので staging/transfer はまだやらない
//--------------------------------------------------------------
static bool CreateBuffer_HostVisible(VkPhysicalDevice phys,
                                    VkDevice device,
                                    VkDeviceSize sizeBytes,
                                    VkBufferUsageFlags usage,
                                    VkBuffer& outBuf,
                                    VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = sizeBytes;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, outBuf, &req);

    const uint32_t memType =
        FindMemoryType(phys, req.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX)
    {
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        return false;
    }

    if (vkBindBufferMemory(device, outBuf, outMem, 0) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// 画像作成（最小：1x1 RGBA8 白）
//  ※最短で表示したいので staging/transfer はまだやらない
//  ※VK_IMAGE_TILING_LINEAR + HOST_VISIBLE を使う（実装依存で遅い/不可なGPUもある）
//    → 次の段階で staging + OPTIMAL に変えるのが正道
//--------------------------------------------------------------
static bool CreateImage1x1White_Linear(VkPhysicalDevice phys,
                                      VkDevice device,
                                      VkImage& outImg,
                                      VkDeviceMemory& outMem,
                                      VkImageView& outView)
{
    outImg  = VK_NULL_HANDLE;
    outMem  = VK_NULL_HANDLE;
    outView = VK_NULL_HANDLE;

    VkImageCreateInfo ici{};
    ici.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format    = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent    = { 1, 1, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples   = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling    = VK_IMAGE_TILING_LINEAR; // ★最短のため
    ici.usage     = VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    if (vkCreateImage(device, &ici, nullptr, &outImg) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, outImg, &req);

    const uint32_t memType =
        FindMemoryType(phys, req.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX)
    {
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        return false;
    }

    if (vkBindImageMemory(device, outImg, outMem, 0) != VK_SUCCESS)
    {
        return false;
    }

    // 1x1 白を書き込む（Linear tiling の rowPitch に注意）
    VkImageSubresource sub{};
    sub.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    sub.mipLevel   = 0;
    sub.arrayLayer = 0;

    VkSubresourceLayout layout{};
    vkGetImageSubresourceLayout(device, outImg, &sub, &layout);

    void* mapped = nullptr;
    if (vkMapMemory(device, outMem, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)
    {
        return false;
    }

    // RGBA8 = 4 bytes
    uint8_t* ptr = (uint8_t*)mapped;
    ptr[layout.offset + 0] = 255;
    ptr[layout.offset + 1] = 255;
    ptr[layout.offset + 2] = 255;
    ptr[layout.offset + 3] = 255;

    vkUnmapMemory(device, outMem);

    // ImageView
    VkImageViewCreateInfo vci{};
    vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image    = outImg;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.baseMipLevel   = 0;
    vci.subresourceRange.levelCount     = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &vci, nullptr, &outView) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// 画像レイアウト遷移（最小）
//  BeginFrame で renderpass を開く前に一回だけ行いたいが、
//  今回のテストは “PREINITIALIZED/LINEAR” なので最小で OK。
//--------------------------------------------------------------
static void CmdTransitionImageLayout(VkCommandBuffer cmd,
                                    VkImage img,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout)
{
    VkImageMemoryBarrier bar{};
    bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.oldLayout = oldLayout;
    bar.newLayout = newLayout;
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.image = img;
    bar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bar.subresourceRange.baseMipLevel = 0;
    bar.subresourceRange.levelCount = 1;
    bar.subresourceRange.baseArrayLayer = 0;
    bar.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    // 超最小：雑に
    bar.srcAccessMask = 0;
    bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         srcStage, dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &bar);
}

//--------------------------------------------------------------
// UIリソース作成（テスト）
//--------------------------------------------------------------
bool VKRenderer::EnsureUIResources()
{
    if (mUiReady)
    {
        return true;
    }

    if (!mDevice || !mPhysicalDevice || !mRenderPass)
    {
        std::cerr << "[VKRenderer] EnsureUIResources failed: device/phys/renderpass missing.\n";
        return false;
    }

    //----------------------------------------------------------
    // (1) DescriptorSetLayout (set=0,binding=0 sampler2D)
    //----------------------------------------------------------
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding            = 0;
        b.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount    = 1;
        b.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 1;
        ci.pBindings    = &b;

        if (vkCreateDescriptorSetLayout(mDevice, &ci, nullptr, &mUiSetLayout) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateDescriptorSetLayout failed.\n";
            return false;
        }
    }

    //----------------------------------------------------------
    // (2) Sampler
    //----------------------------------------------------------
    {
        VkSamplerCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxAnisotropy = 1.0f;

        if (vkCreateSampler(mDevice, &sci, nullptr, &mUiSampler) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateSampler failed.\n";
            return false;
        }
    }

    //----------------------------------------------------------
    // (3) Test Texture (1x1 white)
    //----------------------------------------------------------
    if (!CreateImage1x1White_Linear(mPhysicalDevice, mDevice,
                                   mUiTestImage, mUiTestImageMemory, mUiTestImageView))
    {
        std::cerr << "[VKRenderer] CreateImage1x1White_Linear failed.\n";
        return false;
    }

    //----------------------------------------------------------
    // (4) Descriptor Pool + Sets（swapchain image数分）
    //----------------------------------------------------------
    {
        const uint32_t scCount = (uint32_t)mSwapchainImageViews.size();
        if (scCount == 0)
        {
            std::cerr << "[VKRenderer] EnsureUIResources: swapchain views empty.\n";
            return false;
        }

        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = scCount;

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.maxSets       = scCount;
        pci.poolSizeCount = 1;
        pci.pPoolSizes    = &ps;

        if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mUiDescPool) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateDescriptorPool failed.\n";
            return false;
        }

        mUiDescSets.resize(scCount, VK_NULL_HANDLE);

        std::vector<VkDescriptorSetLayout> layouts(scCount, mUiSetLayout);

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mUiDescPool;
        ai.descriptorSetCount = scCount;
        ai.pSetLayouts        = layouts.data();

        if (vkAllocateDescriptorSets(mDevice, &ai, mUiDescSets.data()) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkAllocateDescriptorSets failed.\n";
            return false;
        }

        // 全セット同じ（白テク）
        for (uint32_t i = 0; i < scCount; ++i)
        {
            VkDescriptorImageInfo ii{};
            ii.sampler     = mUiSampler;
            ii.imageView   = mUiTestImageView;
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = mUiDescSets[i];
            w.dstBinding      = 0;
            w.descriptorCount = 1;
            w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo      = &ii;

            vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
        }
    }

    //----------------------------------------------------------
    // (5) PipelineLayout + Pipeline
    //----------------------------------------------------------
    {
        // push constant: mat4 + vec4（サイズは 16*4 + 4*4 = 80 bytes）
        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset     = 0;
        pcr.size       = sizeof(Matrix4) + sizeof(float) * 4;

        VkPipelineLayoutCreateInfo lci{};
        lci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        lci.setLayoutCount = 1;
        lci.pSetLayouts    = &mUiSetLayout;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges    = &pcr;

        if (vkCreatePipelineLayout(mDevice, &lci, nullptr, &mUiPipelineLayout) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreatePipelineLayout failed.\n";
            return false;
        }

        // Shader modules
        std::vector<uint8_t> vcode, fcode;

        // あなたの運用に合わせて：実行時の作業ディレクトリに依存せず、
        // build 出力先に ToyLib/Shaders をコピーしてある前提
        const std::string vpath = "ToyLib/Shaders/VK/UI_Sprite.vert.spv";
        const std::string fpath = "ToyLib/Shaders/VK/UI_Sprite.frag.spv";

        if (!ReadFileBinary(vpath, vcode) || !ReadFileBinary(fpath, fcode))
        {
            std::cerr << "[VKRenderer] UI shader .spv not found.\n"
                      << "  expected: " << vpath << "\n"
                      << "            " << fpath << "\n";
            return false;
        }

        VkShaderModule vmod = CreateShaderModule(mDevice, vcode);
        VkShaderModule fmod = CreateShaderModule(mDevice, fcode);
        if (!vmod || !fmod)
        {
            std::cerr << "[VKRenderer] CreateShaderModule failed.\n";
            return false;
        }

        VkPipelineShaderStageCreateInfo vs{};
        vs.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vs.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vs.module = vmod;
        vs.pName  = "main";

        VkPipelineShaderStageCreateInfo fs{};
        fs.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fs.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fs.module = fmod;
        fs.pName  = "main";

        VkPipelineShaderStageCreateInfo stages[] = { vs, fs };

        // Vertex: vec2 pos + vec2 uv
        VkVertexInputBindingDescription bind{};
        bind.binding   = 0;
        bind.stride    = sizeof(float) * 4;
        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[2]{};
        attrs[0].location = 0;
        attrs[0].binding  = 0;
        attrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[0].offset   = 0;

        attrs[1].location = 1;
        attrs[1].binding  = 0;
        attrs[1].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset   = sizeof(float) * 2;

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount   = 1;
        vi.pVertexBindingDescriptions      = &bind;
        vi.vertexAttributeDescriptionCount = 2;
        vi.pVertexAttributeDescriptions    = attrs;

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
        rs.cullMode    = VK_CULL_MODE_NONE; // UIなので
        rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Alpha blend
        VkPipelineColorBlendAttachmentState cbAtt{};
        cbAtt.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cbAtt.blendEnable         = VK_TRUE;
        cbAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbAtt.colorBlendOp        = VK_BLEND_OP_ADD;
        cbAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbAtt.alphaBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments    = &cbAtt;

        // Dynamic states: viewport + scissor
        VkDynamicState dyns[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dyns;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable  = VK_FALSE;
        ds.depthWriteEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo gpci{};
        gpci.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount = 2;
        gpci.pStages    = stages;
        gpci.pVertexInputState   = &vi;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState      = &vp;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState   = &ms;
        gpci.pDepthStencilState  = &ds;
        gpci.pColorBlendState    = &cb;
        gpci.pDynamicState       = &dyn;
        gpci.layout     = mUiPipelineLayout;
        gpci.renderPass = mRenderPass;
        gpci.subpass    = 0;

        if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gpci, nullptr, &mUiPipeline) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateGraphicsPipelines(UI) failed.\n";
            vkDestroyShaderModule(mDevice, vmod, nullptr);
            vkDestroyShaderModule(mDevice, fmod, nullptr);
            return false;
        }

        vkDestroyShaderModule(mDevice, vmod, nullptr);
        vkDestroyShaderModule(mDevice, fmod, nullptr);
    }

    //----------------------------------------------------------
    // (6) Test Quad VB/IB（NDCで中央に出す）
    //----------------------------------------------------------
    {
        // pos(x,y), uv(u,v)
        const float verts[] =
        {
            //  x     y     u    v
            -0.5f, -0.5f,  0.0f, 1.0f,
             0.5f, -0.5f,  1.0f, 1.0f,
             0.5f,  0.5f,  1.0f, 0.0f,
            -0.5f,  0.5f,  0.0f, 0.0f,
        };
        const uint32_t indices[] =
        {
            0, 1, 2,
            2, 3, 0
        };
        mUiIndexCount = 6;

        const VkDeviceSize vbSize = sizeof(verts);
        const VkDeviceSize ibSize = sizeof(indices);

        if (!CreateBuffer_HostVisible(mPhysicalDevice, mDevice, vbSize,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     mUiVB, mUiVBMemory))
        {
            std::cerr << "[VKRenderer] CreateBuffer(VB) failed.\n";
            return false;
        }

        if (!CreateBuffer_HostVisible(mPhysicalDevice, mDevice, ibSize,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     mUiIB, mUiIBMemory))
        {
            std::cerr << "[VKRenderer] CreateBuffer(IB) failed.\n";
            return false;
        }

        // upload
        {
            void* p = nullptr;
            vkMapMemory(mDevice, mUiVBMemory, 0, vbSize, 0, &p);
            std::memcpy(p, verts, (size_t)vbSize);
            vkUnmapMemory(mDevice, mUiVBMemory);
        }
        {
            void* p = nullptr;
            vkMapMemory(mDevice, mUiIBMemory, 0, ibSize, 0, &p);
            std::memcpy(p, indices, (size_t)ibSize);
            vkUnmapMemory(mDevice, mUiIBMemory);
        }
    }

    mUiReady = true;
    return true;
}

//--------------------------------------------------------------
// UIリソース破棄
//--------------------------------------------------------------
void VKRenderer::DestroyUIResources()
{
    if (!mDevice)
    {
        mUiReady = false;
        return;
    }

    if (mUiPipeline)
    {
        vkDestroyPipeline(mDevice, mUiPipeline, nullptr);
        mUiPipeline = VK_NULL_HANDLE;
    }
    if (mUiPipelineLayout)
    {
        vkDestroyPipelineLayout(mDevice, mUiPipelineLayout, nullptr);
        mUiPipelineLayout = VK_NULL_HANDLE;
    }

    if (mUiDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mUiDescPool, nullptr);
        mUiDescPool = VK_NULL_HANDLE;
    }
    mUiDescSets.clear();

    if (mUiSetLayout)
    {
        vkDestroyDescriptorSetLayout(mDevice, mUiSetLayout, nullptr);
        mUiSetLayout = VK_NULL_HANDLE;
    }

    if (mUiSampler)
    {
        vkDestroySampler(mDevice, mUiSampler, nullptr);
        mUiSampler = VK_NULL_HANDLE;
    }

    if (mUiTestImageView)
    {
        vkDestroyImageView(mDevice, mUiTestImageView, nullptr);
        mUiTestImageView = VK_NULL_HANDLE;
    }
    if (mUiTestImage)
    {
        vkDestroyImage(mDevice, mUiTestImage, nullptr);
        mUiTestImage = VK_NULL_HANDLE;
    }
    if (mUiTestImageMemory)
    {
        vkFreeMemory(mDevice, mUiTestImageMemory, nullptr);
        mUiTestImageMemory = VK_NULL_HANDLE;
    }

    if (mUiVB)
    {
        vkDestroyBuffer(mDevice, mUiVB, nullptr);
        mUiVB = VK_NULL_HANDLE;
    }
    if (mUiVBMemory)
    {
        vkFreeMemory(mDevice, mUiVBMemory, nullptr);
        mUiVBMemory = VK_NULL_HANDLE;
    }

    if (mUiIB)
    {
        vkDestroyBuffer(mDevice, mUiIB, nullptr);
        mUiIB = VK_NULL_HANDLE;
    }
    if (mUiIBMemory)
    {
        vkFreeMemory(mDevice, mUiIBMemory, nullptr);
        mUiIBMemory = VK_NULL_HANDLE;
    }

    mUiIndexCount = 0;
    mUiReady = false;
}

//--------------------------------------------------------------
// VKRenderer::DrawUIPass
//--------------------------------------------------------------
void VKRenderer::DrawUIPass()
{
    // BeginFrame() の中で vkCmdBeginRenderPass 済み、という前提。
    FrameSync& frame = mFrames[mFrameIndex];
    if (!frame.cmd || !mDevice)
    {
        return;
    }

    // まだ renderpass / framebuffers が出来てない段階なら、ここには来ない想定だが保険
    if (!mRenderPass || mFramebuffers.empty())
    {
        return;
    }

    // 初回にUIリソース生成
    if (!EnsureUIResources())
    {
        return;
    }

    // テスト用：最小 viewport/scissor をセット
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = (float)mSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = mSwapchainExtent;

    vkCmdSetViewport(frame.cmd, 0, 1, &vp);
    vkCmdSetScissor(frame.cmd, 0, 1, &sc);

    // pipeline
    vkCmdBindPipeline(frame.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mUiPipeline);

    // descriptor set（imageIndexに対応）
    const uint32_t idx = (mImageIndex < (uint32_t)mUiDescSets.size()) ? mImageIndex : 0;
    VkDescriptorSet set0 = mUiDescSets[idx];
    vkCmdBindDescriptorSets(frame.cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            mUiPipelineLayout,
                            0, 1, &set0,
                            0, nullptr);

    // VB/IB
    VkDeviceSize vbOff = 0;
    vkCmdBindVertexBuffers(frame.cmd, 0, 1, &mUiVB, &vbOff);
    vkCmdBindIndexBuffer(frame.cmd, mUiIB, 0, VK_INDEX_TYPE_UINT32);

    // Push constants: MVP + Color
    struct Push
    {
        Matrix4 mvp;
        float   color[4];
    };

    Push pc{};
    pc.mvp = Matrix4::Identity; // NDC直書きなので identity でOK
    pc.color[0] = 1.0f;
    pc.color[1] = 1.0f;
    pc.color[2] = 1.0f;
    pc.color[3] = 1.0f;

    vkCmdPushConstants(frame.cmd,
                       mUiPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(Push), &pc);

    // draw
    vkCmdDrawIndexed(frame.cmd, mUiIndexCount, 1, 0, 0, 0);

    // ここまでで「白いQuad」が出れば土台OK
    // 次：VertexArrayBackend(VK) でVB/IBを差し替え、TextureBackend(VK) で set0 を差し替える
}

} // namespace toy
