//======================================================================
// Render/VK/VKRenderer_Shadow.cpp
//
//  - Vulkan Shadow Mapping (Depth-only, 2 cascades like GL)
//  - Shadow resources:
//      * 2 depth images (one per cascade)  [simple & safe]
//      * depth-only render pass + framebuffer per cascade
//      * comparison sampler for sampling later
//
//  - Shadow scene UBO (set=0 binding=0):
//      uLightVP (per cascade). We update per-cascade before drawing.
//
//  IMPORTANT:
//    - Bucket traversal is done by IRenderer::DrawBucket_Shadow()
//      (which calls VKRenderer::DrawItem(it, Shadow, cascadeIndex)).
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKUtil.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/LightingManager.h"

#include <iostream>
#include <cstring>
#include <algorithm>

namespace toy
{

static constexpr int kShadowCascadeCount = 2;

//--------------------------------------------------------------
// Shadow scene UBO (minimal)
//--------------------------------------------------------------
struct VKShadowSceneUBO
{
    float lightVP[16];
};

//--------------------------------------------------------------
// Depth format helper
//--------------------------------------------------------------
static VkFormat ChooseDepthFormat(VkPhysicalDevice phys)
{
    const VkFormat candidates[] =
    {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat f : candidates)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(phys, f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return f;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

static bool HasStencil(VkFormat f)
{
    return (f == VK_FORMAT_D32_SFLOAT_S8_UINT) || (f == VK_FORMAT_D24_UNORM_S8_UINT);
}

static VkImageAspectFlags DepthAspect(VkFormat f)
{
    VkImageAspectFlags a = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencil(f)) a |= VK_IMAGE_ASPECT_STENCIL_BIT;
    return a;
}

//--------------------------------------------------------------
// Bias matrix for NDC->UV (row-vector convention)
//--------------------------------------------------------------
static Matrix4 MakeShadowBias_RowVector()
{
    Matrix4 b = Matrix4::Identity;

    // row-vector mapping:
    // [0.5 0   0   0]
    // [0   0.5 0   0]
    // [0   0   1   0]
    // [0.5 0.5 0   1]
    //
    // NOTE: adjust indexing if your Matrix4 storage differs.
    b.mat[0][0] = 0.5f;
    b.mat[1][1] = 0.5f;
    b.mat[2][2] = 1.0f;
    b.mat[3][0] = 0.5f;
    b.mat[3][1] = 0.5f;
    b.mat[3][3] = 1.0f;

    return b;
}

//--------------------------------------------------------------
// Shadow resources
//--------------------------------------------------------------
bool VKRenderer::CreateShadowResources()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }
    if (!mDescPool)
    {
        std::cerr << "[VKRenderer] Shadow: descriptor pool is null.\n";
        return false;
    }

    const uint32_t w = (uint32_t)std::max(1, mShadowFBOWidth);
    const uint32_t h = (uint32_t)std::max(1, mShadowFBOHeight);

    // already ok?
    if (mShadowExtent.width == w && mShadowExtent.height == h &&
        mShadowDepthFormat != VK_FORMAT_UNDEFINED &&
        mShadowRenderPass != VK_NULL_HANDLE &&
        mShadowSampler != VK_NULL_HANDLE &&
        (int)mShadowCascades.size() == kShadowCascadeCount &&
        !mShadowSceneUBO.empty() &&
        !mShadowSceneSet.empty())
    {
        return true;
    }

    DestroyShadowResources();

    mShadowExtent = { w, h };
    mShadowDepthFormat = ChooseDepthFormat(mPhysicalDevice);
    if (mShadowDepthFormat == VK_FORMAT_UNDEFINED)
    {
        std::cerr << "[VKRenderer] Shadow: no supported depth format.\n";
        return false;
    }

    //----------------------------------------------------------
    // Shadow render pass (depth-only)
    //----------------------------------------------------------
    VkAttachmentDescription depthAtt{};
    depthAtt.format         = mShadowDepthFormat;
    depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE; // sample later
    depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 0;
    sub.pColorAttachments       = nullptr;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &depthAtt;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    if (vkCreateRenderPass(mDevice, &rpci, nullptr, &mShadowRenderPass) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] Shadow: vkCreateRenderPass failed.\n";
        DestroyShadowResources();
        return false;
    }

    //----------------------------------------------------------
    // Sampler (comparison)
    //----------------------------------------------------------
    VkSamplerCreateInfo sci{};
    sci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter               = VK_FILTER_LINEAR;
    sci.minFilter               = VK_FILTER_LINEAR;
    sci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.anisotropyEnable        = VK_FALSE;
    sci.maxAnisotropy           = 1.0f;
    sci.compareEnable           = VK_TRUE;
    sci.compareOp               = VK_COMPARE_OP_LESS_OR_EQUAL;
    sci.minLod                  = 0.0f;
    sci.maxLod                  = 0.0f;
    sci.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sci.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(mDevice, &sci, nullptr, &mShadowSampler) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] Shadow: vkCreateSampler failed.\n";
        DestroyShadowResources();
        return false;
    }

    //----------------------------------------------------------
    // Cascades (2 images)
    //----------------------------------------------------------
    mShadowCascades.resize(kShadowCascadeCount);

    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        ShadowCascade& c = mShadowCascades[i];

        if (!toy::vkutil::CreateImage2D(
                mPhysicalDevice,
                mDevice,
                w,
                h,
                mShadowDepthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                c.depthImg,
                c.depthMem,
                VK_IMAGE_LAYOUT_UNDEFINED))
        {
            std::cerr << "[VKRenderer] Shadow: CreateImage2D failed cascade=" << i << "\n";
            DestroyShadowResources();
            return false;
        }

        c.depthView = toy::vkutil::CreateImageView2D(
            mDevice,
            c.depthImg,
            mShadowDepthFormat,
            DepthAspect(mShadowDepthFormat));

        if (c.depthView == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] Shadow: CreateImageView2D failed cascade=" << i << "\n";
            DestroyShadowResources();
            return false;
        }

        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = mShadowRenderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &c.depthView;
        fbci.width           = w;
        fbci.height          = h;
        fbci.layers          = 1;

        if (vkCreateFramebuffer(mDevice, &fbci, nullptr, &c.fb) != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] Shadow: vkCreateFramebuffer failed cascade=" << i << "\n";
            DestroyShadowResources();
            return false;
        }

        // pre-transition (safe)
        VkCommandBuffer cmd = BeginOneTimeCommands();
        if (cmd)
        {
            toy::vkutil::CmdTransitionImageLayout(
                cmd,
                c.depthImg,
                DepthAspect(mShadowDepthFormat),
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

            EndOneTimeCommands(cmd);
        }

        c.lightVP = Matrix4::Identity;
        c.lightVP_Biased = Matrix4::Identity;
    }

    //----------------------------------------------------------
    // Shadow scene UBO + set=0
    //----------------------------------------------------------
    if (!CreateShadowSceneUBOAndSet())
    {
        DestroyShadowResources();
        return false;
    }

    mShadowBias = MakeShadowBias_RowVector();

    return true;
}

void VKRenderer::DestroyShadowResources()
{
    if (!mDevice)
    {
        mShadowCascades.clear();
        mShadowSceneUBO.clear();
        mShadowSceneUBOMem.clear();
        mShadowSceneSet.clear();
        mShadowRenderPass = VK_NULL_HANDLE;
        mShadowSampler = VK_NULL_HANDLE;
        mShadowExtent = {0,0};
        mShadowDepthFormat = VK_FORMAT_UNDEFINED;
        mShadowBias = Matrix4::Identity;
        return;
    }

    DestroyShadowSceneUBOAndSet();

    for (auto& c : mShadowCascades)
    {
        if (c.fb) vkDestroyFramebuffer(mDevice, c.fb, nullptr);
        c.fb = VK_NULL_HANDLE;

        if (c.depthView) vkDestroyImageView(mDevice, c.depthView, nullptr);
        c.depthView = VK_NULL_HANDLE;

        if (c.depthImg) vkDestroyImage(mDevice, c.depthImg, nullptr);
        c.depthImg = VK_NULL_HANDLE;

        if (c.depthMem) vkFreeMemory(mDevice, c.depthMem, nullptr);
        c.depthMem = VK_NULL_HANDLE;

        c.lightVP = Matrix4::Identity;
        c.lightVP_Biased = Matrix4::Identity;
    }
    mShadowCascades.clear();

    if (mShadowSampler)
    {
        vkDestroySampler(mDevice, mShadowSampler, nullptr);
        mShadowSampler = VK_NULL_HANDLE;
    }

    if (mShadowRenderPass)
    {
        vkDestroyRenderPass(mDevice, mShadowRenderPass, nullptr);
        mShadowRenderPass = VK_NULL_HANDLE;
    }

    mShadowExtent = {0,0};
    mShadowDepthFormat = VK_FORMAT_UNDEFINED;
    mShadowBias = Matrix4::Identity;
}

//--------------------------------------------------------------
// Shadow scene UBO + set=0
//--------------------------------------------------------------
bool VKRenderer::CreateShadowSceneUBOAndSet()
{
    const size_t frameCount = mFrames.size();
    if (frameCount == 0)
    {
        return false;
    }

    mShadowSceneUBO.assign(frameCount, VK_NULL_HANDLE);
    mShadowSceneUBOMem.assign(frameCount, VK_NULL_HANDLE);
    mShadowSceneSet.assign(frameCount, VK_NULL_HANDLE);

    // set0 layout: assume same contract as Sprite set0 (like your World/UI code)
    VkDescriptorSetLayout set0 = VK_NULL_HANDLE;
    {
        auto* p = mPipelines.Get("Sprite");
        if (p) set0 = p->GetSetLayout(0);
    }
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] Shadow: set0 layout null (Sprite pipeline missing?)\n";
        return false;
    }

    const VkDeviceSize uboSize = sizeof(VKShadowSceneUBO);

    for (size_t i = 0; i < frameCount; ++i)
    {
        if (!CreateBufferHostVisible(uboSize,
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    mShadowSceneUBO[i],
                                    mShadowSceneUBOMem[i]))
        {
            std::cerr << "[VKRenderer] Shadow: CreateBufferHostVisible failed frame=" << i << "\n";
            return false;
        }

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &set0;

        if (vkAllocateDescriptorSets(mDevice, &ai, &mShadowSceneSet[i]) != VK_SUCCESS ||
            mShadowSceneSet[i] == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] Shadow: alloc shadow scene set failed frame=" << i << "\n";
            return false;
        }

        VkDescriptorBufferInfo bi{};
        bi.buffer = mShadowSceneUBO[i];
        bi.offset = 0;
        bi.range  = uboSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = mShadowSceneSet[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    }

    return true;
}

void VKRenderer::DestroyShadowSceneUBOAndSet()
{
    if (!mDevice)
    {
        mShadowSceneUBO.clear();
        mShadowSceneUBOMem.clear();
        mShadowSceneSet.clear();
        return;
    }

    if (mDescPool)
    {
        for (auto& ds : mShadowSceneSet)
        {
            if (ds) vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            ds = VK_NULL_HANDLE;
        }
    }
    mShadowSceneSet.clear();

    for (size_t i = 0; i < mShadowSceneUBO.size(); ++i)
    {
        if (mShadowSceneUBO[i]) vkDestroyBuffer(mDevice, mShadowSceneUBO[i], nullptr);
        mShadowSceneUBO[i] = VK_NULL_HANDLE;

        if (mShadowSceneUBOMem[i]) vkFreeMemory(mDevice, mShadowSceneUBOMem[i], nullptr);
        mShadowSceneUBOMem[i] = VK_NULL_HANDLE;
    }
    mShadowSceneUBO.clear();
    mShadowSceneUBOMem.clear();
}

//--------------------------------------------------------------
// Light matrices (GL と同じ発想で2カスケード)
//--------------------------------------------------------------
void VKRenderer::UpdateShadowLightMatrices()
{
    if ((int)mShadowCascades.size() != kShadowCascadeCount)
    {
        return;
    }

    Vector3 camPos = GetCameraPosition();

    //Vector3 camForward = GetCameraForward(); // 無ければ mInvView.GetZAxis() 相当で
    Vector3 camForward = mInvView.GetZAxis();
    if (camForward.LengthSq() < 1.0e-6f)
    {
        camForward = Vector3(0, 0, 1);
    }
    camForward.Normalize();

    Vector3 camCenter = camPos + camForward * 30.0f;

    DirectionalLight dir{};
    if (auto lm = GetLightingManager())
    {
        dir = lm->GetDirectionalLight();
    }
    Vector3 lightDir = dir.GetDirection();
    if (lightDir.LengthSq() < 1.0e-6f)
    {
        lightDir = Vector3(0, -1, 0);
    }
    lightDir.Normalize();

    Vector3 lightPos = camCenter - lightDir * 50.0f;

    Matrix4 lightView = Matrix4::CreateLookAt(lightPos, camCenter, Vector3::UnitY);

    const float orthoW[kShadowCascadeCount] =
    {
        mShadowOrthoWidth,
        mShadowOrthoWidth * 4.0f
    };
    const float orthoH[kShadowCascadeCount] =
    {
        mShadowOrthoHeight,
        mShadowOrthoHeight * 4.0f
    };

    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        Matrix4 lightProj = Matrix4::CreateOrtho(
            orthoW[i],
            orthoH[i],
            mShadowNear,
            mShadowFar);

        Matrix4 lightVP = lightView * lightProj;

        mShadowCascades[i].lightVP        = lightVP;
        mShadowCascades[i].lightVP_Biased = lightVP * mShadowBias;
    }
}

void VKRenderer::UpdateShadowSceneUBO(int cascadeIndex)
{
    if (cascadeIndex < 0 || cascadeIndex >= (int)mShadowCascades.size())
    {
        return;
    }
    if (mShadowSceneUBOMem.empty() || mFrameIndex >= mShadowSceneUBOMem.size())
    {
        return;
    }

    VKShadowSceneUBO ubo{};
    std::memcpy(ubo.lightVP, &mShadowCascades[cascadeIndex].lightVP, sizeof(float) * 16);

    UploadToBuffer(mShadowSceneUBOMem[mFrameIndex], &ubo, sizeof(VKShadowSceneUBO));
}

//--------------------------------------------------------------
// Shadow pass
//--------------------------------------------------------------
void VKRenderer::DrawShadowPass()
{
    if (!mDevice || mFrames.empty())
    {
        return;
    }

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (!cmd)
    {
        return;
    }

    if (!CreateShadowResources())
    {
        return;
    }

    UpdateShadowLightMatrices();

    for (int c = 0; c < kShadowCascadeCount; ++c)
    {
        ShadowCascade& cas = mShadowCascades[c];

        // Update lightVP in UBO (per cascade)
        UpdateShadowSceneUBO(c);

        VkClearValue clear{};
        clear.depthStencil.depth   = 1.0f;
        clear.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp{};
        rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass  = mShadowRenderPass;
        rp.framebuffer = cas.fb;
        rp.renderArea.offset = { 0, 0 };
        rp.renderArea.extent = mShadowExtent;
        rp.clearValueCount   = 1;
        rp.pClearValues      = &clear;

        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width    = (float)mShadowExtent.width;
        vp.height   = (float)mShadowExtent.height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.offset = { 0, 0 };
        sc.extent = mShadowExtent;
        vkCmdSetScissor(cmd, 0, 1, &sc);

        // ★ここが核心：bucket traversal は IRenderer がやる
        DrawBucket_Shadow(mBuckets.shadowCaster, c);

        vkCmdEndRenderPass(cmd);
    }
}

//--------------------------------------------------------------
// After shadow pass: transition depth to sampled
//--------------------------------------------------------------
void VKRenderer::RestoreAfterShadowPass()
{
    if (!mDevice || mShadowCascades.empty())
    {
        return;
    }
    if (mFrames.empty() || !mFrames[mFrameIndex].cmd)
    {
        return;
    }

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;

    for (auto& c : mShadowCascades)
    {
        toy::vkutil::CmdTransitionImageLayout(
            cmd,
            c.depthImg,
            DepthAspect(mShadowDepthFormat),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
    }
}

} // namespace toy
