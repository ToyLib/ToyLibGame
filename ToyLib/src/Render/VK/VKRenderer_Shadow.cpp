//======================================================================
// Render/VK/VKRenderer_Shadow.cpp
//
//  - Vulkan Shadow Mapping (Depth-only, 2 cascades like GL)
//  - Shadow resources:
//      * 2 depth images (one per cascade)
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

    // ここはあなたの既存メンバに合わせる（mShadowFBOWidth/Height が既にある前提）
    const uint32_t w = (uint32_t)std::max(1, mShadowFBOWidth);
    const uint32_t h = (uint32_t)std::max(1, mShadowFBOHeight);

    const size_t frameCount = mFrames.size();

    auto ShadowSceneReady = [&]() -> bool
    {
        if (frameCount == 0) return false;

        for (int ci = 0; ci < kShadowCascadeCount; ++ci)
        {
            if (mShadowSceneUBO[ci].size()    != frameCount) return false;
            if (mShadowSceneUBOMem[ci].size() != frameCount) return false;
            if (mShadowSceneSet[ci].size()    != frameCount) return false;

            for (size_t fi = 0; fi < frameCount; ++fi)
            {
                if (mShadowSceneUBO[ci][fi]    == VK_NULL_HANDLE) return false;
                if (mShadowSceneUBOMem[ci][fi] == VK_NULL_HANDLE) return false;
                if (mShadowSceneSet[ci][fi]    == VK_NULL_HANDLE) return false;
            }
        }
        return true;
    };

    // already ok?
    if (mShadowExtent.width == w && mShadowExtent.height == h &&
        mShadowDepthFormat != VK_FORMAT_UNDEFINED &&
        mShadowRenderPass != VK_NULL_HANDLE &&
        mShadowSampler != VK_NULL_HANDLE &&
        (int)mShadowCascades.size() == kShadowCascadeCount &&
        ShadowSceneReady() &&
        !mShadowMapSet.empty())
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
    }

    //----------------------------------------------------------
    // Shadow scene UBO + set=0  （★cascade別に作る）
    //----------------------------------------------------------
    if (!CreateShadowSceneUBOAndSet())
    {
        DestroyShadowResources();
        return false;
    }

    mShadowIsSampledLayout = { false, false };

    //----------------------------------------------------------
    // Shadow sampled descriptor (set=3)  （★Mesh pipeline の set=3 layout を使用）
    //----------------------------------------------------------
    if (!CreateShadowSampleSet())
    {
        DestroyShadowResources();
        return false;
    }
    UpdateShadowSampleSet();

    mShadowDescPoolUsed = mDescPool;

    return true;
}

void VKRenderer::DestroyShadowResources()
{
    if (!mDevice)
    {
        mShadowCascades.clear();

        for (int ci = 0; ci < kShadowCascadeCount; ++ci)
        {
            mShadowSceneUBO[ci].clear();
            mShadowSceneUBOMem[ci].clear();
            mShadowSceneSet[ci].clear();
        }

        mShadowRenderPass = VK_NULL_HANDLE;
        mShadowSampler = VK_NULL_HANDLE;
        mShadowExtent = {0,0};
        mShadowDepthFormat = VK_FORMAT_UNDEFINED;
        return;
    }

    mShadowDescPoolUsed = VK_NULL_HANDLE;

    DestroyShadowMapSetLayoutAndSets();
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
}

//--------------------------------------------------------------
// Shadow scene UBO + set=0  （★cascade別）
//--------------------------------------------------------------
bool VKRenderer::CreateShadowSceneUBOAndSet()
{
    const size_t frameCount = mFrames.size();
    if (frameCount == 0) return false;

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

    for (int ci = 0; ci < kShadowCascadeCount; ++ci)
    {
        mShadowSceneUBO[ci].assign(frameCount, VK_NULL_HANDLE);
        mShadowSceneUBOMem[ci].assign(frameCount, VK_NULL_HANDLE);
        mShadowSceneSet[ci].assign(frameCount, VK_NULL_HANDLE);

        for (size_t fi = 0; fi < frameCount; ++fi)
        {
            if (!CreateBufferHostVisible(
                    uboSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    mShadowSceneUBO[ci][fi],
                    mShadowSceneUBOMem[ci][fi]))
            {
                std::cerr << "[VKRenderer] Shadow: CreateBufferHostVisible failed cascade="
                          << ci << " frame=" << fi << "\n";
                return false;
            }

            VkDescriptorSetAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool     = mDescPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts        = &set0;

            if (vkAllocateDescriptorSets(mDevice, &ai, &mShadowSceneSet[ci][fi]) != VK_SUCCESS ||
                mShadowSceneSet[ci][fi] == VK_NULL_HANDLE)
            {
                std::cerr << "[VKRenderer] Shadow: alloc shadow scene set failed cascade="
                          << ci << " frame=" << fi << "\n";
                return false;
            }

            VkDescriptorBufferInfo bi{};
            bi.buffer = mShadowSceneUBO[ci][fi];
            bi.offset = 0;
            bi.range  = uboSize;

            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = mShadowSceneSet[ci][fi];
            w.dstBinding      = 0;
            w.descriptorCount = 1;
            w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo     = &bi;

            vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
        }
    }

    return true;
}

void VKRenderer::DestroyShadowSceneUBOAndSet()
{
    if (!mDevice)
    {
        for (int ci = 0; ci < kShadowCascadeCount; ++ci)
        {
            mShadowSceneUBO[ci].clear();
            mShadowSceneUBOMem[ci].clear();
            mShadowSceneSet[ci].clear();
        }
        return;
    }

    // free DS
    if (mDescPool)
    {
        for (int ci = 0; ci < kShadowCascadeCount; ++ci)
        {
            for (auto& ds : mShadowSceneSet[ci])
            {
                if (ds) vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
                ds = VK_NULL_HANDLE;
            }
            mShadowSceneSet[ci].clear();
        }
    }
    else
    {
        for (int ci = 0; ci < kShadowCascadeCount; ++ci)
        {
            mShadowSceneSet[ci].clear();
        }
    }

    // destroy UBOs
    for (int ci = 0; ci < kShadowCascadeCount; ++ci)
    {
        for (size_t fi = 0; fi < mShadowSceneUBO[ci].size(); ++fi)
        {
            if (mShadowSceneUBO[ci][fi]) vkDestroyBuffer(mDevice, mShadowSceneUBO[ci][fi], nullptr);
            mShadowSceneUBO[ci][fi] = VK_NULL_HANDLE;

            if (mShadowSceneUBOMem[ci][fi]) vkFreeMemory(mDevice, mShadowSceneUBOMem[ci][fi], nullptr);
            mShadowSceneUBOMem[ci][fi] = VK_NULL_HANDLE;
        }

        mShadowSceneUBO[ci].clear();
        mShadowSceneUBOMem[ci].clear();
    }
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

    Vector3 camPos = mInvView.GetTranslation();

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
    

    const float base = std::max(mShadowOrthoWidth, mShadowOrthoHeight);
    const float lightDist = base * 2.0f;
    Vector3 lightPos = camCenter - lightDir * lightDist;
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
        mShadowCascades[i].lightVP = lightVP;
    }
}

void VKRenderer::UpdateShadowSceneUBO(int cascadeIndex)
{
    if (cascadeIndex < 0 || cascadeIndex >= (int)mShadowCascades.size())
    {
        return;
    }
    if (mFrames.empty() || mFrameIndex >= mFrames.size())
    {
        return;
    }

    // ★cascade別 [frame] に書く
    if (mShadowSceneUBOMem[cascadeIndex].empty() ||
        mFrameIndex >= mShadowSceneUBOMem[cascadeIndex].size())
    {
        return;
    }

    VKShadowSceneUBO ubo{};
    std::memcpy(ubo.lightVP, &mShadowCascades[cascadeIndex].lightVP, sizeof(float) * 16);

    UploadToBuffer(mShadowSceneUBOMem[cascadeIndex][mFrameIndex], &ubo, sizeof(VKShadowSceneUBO));
}

//--------------------------------------------------------------
// Shadow pass
//--------------------------------------------------------------
void VKRenderer::DrawShadowPass()
{
    if (mDevice == VK_NULL_HANDLE) return;
    if (mFrames.empty()) return;

    if (mShadowRenderPass == VK_NULL_HANDLE) return;
    if ((int)mShadowCascades.size() != kShadowCascadeCount) return;
    if (mShadowExtent.width == 0 || mShadowExtent.height == 0) return;

    // swapchain pass 中なら閉じる（shadowは別renderpass）
    EndSwapchainRenderPassIfNeeded();

    VkCommandBuffer cmd = mFrames[mFrameIndex].cmd;
    if (cmd == VK_NULL_HANDLE) return;

    // ライト行列更新（CPU側で c.lightVP を計算）
    UpdateShadowLightMatrices();

    for (int ci = 0; ci < kShadowCascadeCount; ++ci)
    {
        mShadowCascadeIndex = ci;

        ShadowCascade& c = mShadowCascades[ci];
        if (c.fb == VK_NULL_HANDLE) continue;

        //------------------------------------------------------
        // (A) layout: sampled -> attachment
        //------------------------------------------------------
        if (mShadowIsSampledLayout[ci])
        {
            toy::vkutil::CmdTransitionImageLayout(
                cmd,
                c.depthImg,
                DepthAspect(mShadowDepthFormat),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

            mShadowIsSampledLayout[ci] = false;
        }

        //------------------------------------------------------
        // (B) shadow scene UBO を “このカスケード用” に更新
        //------------------------------------------------------
        UpdateShadowSceneUBO(ci);

        //------------------------------------------------------
        // (C) begin shadow renderpass (depth-only)
        //------------------------------------------------------
        VkClearValue clear{};
        clear.depthStencil.depth = 1.0f;
        clear.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp{};
        rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass  = mShadowRenderPass;
        rp.framebuffer = c.fb;
        rp.renderArea.offset = { 0, 0 };
        rp.renderArea.extent = mShadowExtent;
        rp.clearValueCount   = 1;
        rp.pClearValues      = &clear;

        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width  = (float)mShadowExtent.width;
        vp.height = (float)mShadowExtent.height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.offset = { 0, 0 };
        sc.extent = mShadowExtent;
        vkCmdSetScissor(cmd, 0, 1, &sc);

        //------------------------------------------------------
        // (D) draw shadow casters
        //------------------------------------------------------
        // ※ DrawBucket_Shadow は IRenderer 側で it を回し、
        //    VKRenderer::DrawItem(it, Shadow, cascadeIndex) を呼ぶ想定
        DrawBucket_Shadow(mBuckets.shadowCaster, ci);

        vkCmdEndRenderPass(cmd);

        //------------------------------------------------------
        // (E) layout: attachment -> sampled
        //------------------------------------------------------
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

        mShadowIsSampledLayout[ci] = true;
    }

    TransitionShadowDepthToSampledIfNeeded(cmd);
    mShadowCascadeIndex = -1;
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

    for (int i = 0; i < (int)mShadowCascades.size(); ++i)
    {
        auto& c = mShadowCascades[i];

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

        mShadowIsSampledLayout[i] = true;
    }
}

//--------------------------------------------------------------
// ShadowMap descriptor: set=3
//
// ここは「二重に allocate して上書き」事故が起きやすいので、
//  - CreateShadowMapSetLayoutAndSets() は “互換ラッパー” にして
//  - 実体は CreateShadowSampleSet()/UpdateShadowSampleSet() に寄せる
//--------------------------------------------------------------
bool VKRenderer::CreateShadowMapSetLayoutAndSets()
{
    // 互換：外部から呼ばれても set=3 を作れるようにする
    return CreateShadowSampleSet();
}

void VKRenderer::DestroyShadowMapSetLayoutAndSets()
{
    if (!mDevice)
    {
        mShadowMapSet.clear();
        mShadowMapSetLayout = VK_NULL_HANDLE;
        return;
    }

    // set=3 は pipeline の layout を使う方針なので mShadowMapSetLayout は破棄しない
    mShadowMapSetLayout = VK_NULL_HANDLE;

    if (mDescPool && !mShadowMapSet.empty())
    {
        for (auto& ds : mShadowMapSet)
        {
            if (ds)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
            }
            ds = VK_NULL_HANDLE;
        }
    }
    mShadowMapSet.clear();
}

bool VKRenderer::CreateShadowSampleSet()
{
    if (!mDevice || !mDescPool) return false;

    const size_t frameCount = mFrames.size();
    if (frameCount == 0) return false;

    // Mesh pipeline の set=3 layout を使う（Skinned でも同じでOK）
    VkDescriptorSetLayout set3 = VK_NULL_HANDLE;
    if (auto* p = mPipelines.Get("Mesh"))
    {
        set3 = p->GetSetLayout(3);
    }
    if (set3 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] Shadow: set=3 layout null (Mesh pipeline missing?)\n";
        return false;
    }

    // 既に同数なら再利用（内容は Update で貼り直す）
    if (mShadowMapSet.size() != frameCount)
    {
        // 既存があれば先に破棄（pool枯れ回避）
        DestroyShadowMapSetLayoutAndSets();

        mShadowMapSet.assign(frameCount, VK_NULL_HANDLE);

        for (size_t i = 0; i < frameCount; ++i)
        {
            VkDescriptorSetAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool     = mDescPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts        = &set3;

            if (vkAllocateDescriptorSets(mDevice, &ai, &mShadowMapSet[i]) != VK_SUCCESS ||
                mShadowMapSet[i] == VK_NULL_HANDLE)
            {
                std::cerr << "[VKRenderer] Shadow: alloc set=3 failed frame=" << i << "\n";
                return false;
            }
        }
    }

    return true;
}

void VKRenderer::UpdateShadowSampleSet()
{
    if (mDevice == VK_NULL_HANDLE) return;
    if (mShadowMapSet.empty()) return;
    if ((int)mShadowCascades.size() < kShadowCascadeCount) return;
    if (mShadowSampler == VK_NULL_HANDLE) return;

    // World の shader は VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL で読む前提
    const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (size_t fi = 0; fi < mShadowMapSet.size(); ++fi)
    {
        VkDescriptorSet dst = mShadowMapSet[fi];
        if (dst == VK_NULL_HANDLE) continue;

        VkDescriptorImageInfo img0{};
        img0.imageView   = mShadowCascades[0].depthView;
        img0.sampler     = mShadowSampler;
        img0.imageLayout = layout;

        VkDescriptorImageInfo img1{};
        img1.imageView   = mShadowCascades[1].depthView;
        img1.sampler     = mShadowSampler;
        img1.imageLayout = layout;

        VkWriteDescriptorSet w[2]{};

        // set=3 binding=0 : cascade0
        w[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet          = dst;
        w[0].dstBinding      = 0;
        w[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].descriptorCount = 1;
        w[0].pImageInfo      = &img0;

        // set=3 binding=1 : cascade1
        w[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[1].dstSet          = dst;
        w[1].dstBinding      = 1;
        w[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = 1;
        w[1].pImageInfo      = &img1;

        vkUpdateDescriptorSets(mDevice, 2, w, 0, nullptr);
    }
}

void VKRenderer::TransitionShadowDepthToSampledIfNeeded(VkCommandBuffer cmd)
{
    if (!cmd) return;
    if ((int)mShadowCascades.size() < kShadowCascadeCount) return;

    for (int ci = 0; ci < kShadowCascadeCount; ++ci)
    {
        if (mShadowIsSampledLayout[ci]) continue;

        toy::vkutil::CmdTransitionImageLayout(
            cmd,
            mShadowCascades[ci].depthImg,
            DepthAspect(mShadowDepthFormat),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);

        mShadowIsSampledLayout[ci] = true;
    }
}

} // namespace toy
