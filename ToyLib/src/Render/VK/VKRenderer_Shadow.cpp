//======================================================================
// Render/VK/VKRenderer_Shadow.cpp
//  - ShadowMap (Depth-only) resources + passes
//  - Uses IRenderer-held params:
//      mShadowNear / mShadowFar
//      mShadowOrthoWidth / mShadowOrthoHeight
//      mShadowFBOWidth / mShadowFBOHeight
//
//  - Minimal single directional shadow map (no CSM yet)
//
//  Notes:
//   - Row-vector convention assumed in ToyLib (v * M)
//   - LightVP should be built consistently with your Matrix4 utilities
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/VKUtil.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/LightingManager.h"
#include "Engine/Core/Application.h"

#include <iostream>
#include <cstring>
#include <algorithm>

namespace toy
{

//--------------------------------------------------------------
// Helpers
//--------------------------------------------------------------
static VkFormat ChooseShadowDepthFormat(VkPhysicalDevice phys)
{
    // Prefer D32_SFLOAT, fallback to D24_UNORM_S8 etc.
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
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencil(f))
    {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return aspect;
}

//--------------------------------------------------------------
// Shadow resources create/destroy
//--------------------------------------------------------------
bool VKRenderer::CreateShadowResources()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }

    // Resolution from IRenderer-held settings
    const uint32_t w = (uint32_t)std::max(1, mShadowFBOWidth);
    const uint32_t h = (uint32_t)std::max(1, mShadowFBOHeight);

    // If already created with same extent, keep
    if (mShadowDepthImg != VK_NULL_HANDLE &&
        mShadowDepthView != VK_NULL_HANDLE &&
        mShadowSampler != VK_NULL_HANDLE &&
        mShadowRenderPass != VK_NULL_HANDLE &&
        mShadowFB != VK_NULL_HANDLE &&
        mShadowExtent.width == w &&
        mShadowExtent.height == h)
    {
        return true;
    }

    DestroyShadowResources();

    mShadowExtent = { w, h };

    // Format
    mShadowDepthFormat = ChooseShadowDepthFormat(mPhysicalDevice);
    if (mShadowDepthFormat == VK_FORMAT_UNDEFINED)
    {
        std::cerr << "[VKRenderer] Shadow: no supported depth format.\n";
        return false;
    }

    // Depth image
    if (!toy::vkutil::CreateImage2D(
            mPhysicalDevice,
            mDevice,
            w,
            h,
            mShadowDepthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mShadowDepthImg,
            mShadowDepthMem,
            VK_IMAGE_LAYOUT_UNDEFINED))
    {
        std::cerr << "[VKRenderer] Shadow: CreateImage2D failed.\n";
        DestroyShadowResources();
        return false;
    }

    mShadowDepthView = toy::vkutil::CreateImageView2D(
        mDevice,
        mShadowDepthImg,
        mShadowDepthFormat,
        DepthAspect(mShadowDepthFormat));

    if (mShadowDepthView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] Shadow: CreateImageView2D failed.\n";
        DestroyShadowResources();
        return false;
    }

    // Sampler (compare sampler for shadow2D / sampler2DShadow-like usage)
    VkSamplerCreateInfo sci{};
    sci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter               = VK_FILTER_LINEAR;
    sci.minFilter               = VK_FILTER_LINEAR;
    sci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.mipLodBias              = 0.0f;
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

    // RenderPass (depth-only)
    VkAttachmentDescription depthAtt{};
    depthAtt.format         = mShadowDepthFormat;
    depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE; // needed for sampling later
    depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // we'll transition after pass

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 0;
    sub.pColorAttachments       = nullptr;
    sub.pDepthStencilAttachment = &depthRef;

    // Minimal dependencies (we’ll do explicit transition after pass)
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

    // Framebuffer
    VkFramebufferCreateInfo fbci{};
    fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbci.renderPass      = mShadowRenderPass;
    fbci.attachmentCount = 1;
    fbci.pAttachments    = &mShadowDepthView;
    fbci.width           = w;
    fbci.height          = h;
    fbci.layers          = 1;

    if (vkCreateFramebuffer(mDevice, &fbci, nullptr, &mShadowFB) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] Shadow: vkCreateFramebuffer failed.\n";
        DestroyShadowResources();
        return false;
    }

    // Put image in a known layout for first use (optional; renderpass clear will work from UNDEFINED,
    // but having explicit transition helps validation in some stacks)
    {
        VkCommandBuffer cmd = BeginOneTimeCommands();
        if (cmd)
        {
            toy::vkutil::CmdTransitionImageLayout(
                cmd,
                mShadowDepthImg,
                DepthAspect(mShadowDepthFormat),
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

            EndOneTimeCommands(cmd);
        }
    }

    return true;
}

void VKRenderer::DestroyShadowResources()
{
    if (!mDevice)
    {
        mShadowExtent = { 0, 0 };
        mShadowDepthFormat = VK_FORMAT_UNDEFINED;
        mShadowDepthImg = VK_NULL_HANDLE;
        mShadowDepthMem = VK_NULL_HANDLE;
        mShadowDepthView = VK_NULL_HANDLE;
        mShadowSampler = VK_NULL_HANDLE;
        mShadowRenderPass = VK_NULL_HANDLE;
        mShadowFB = VK_NULL_HANDLE;
        return;
    }

    if (mShadowFB != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(mDevice, mShadowFB, nullptr);
        mShadowFB = VK_NULL_HANDLE;
    }
    if (mShadowRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(mDevice, mShadowRenderPass, nullptr);
        mShadowRenderPass = VK_NULL_HANDLE;
    }
    if (mShadowSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(mDevice, mShadowSampler, nullptr);
        mShadowSampler = VK_NULL_HANDLE;
    }
    if (mShadowDepthView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mShadowDepthView, nullptr);
        mShadowDepthView = VK_NULL_HANDLE;
    }
    if (mShadowDepthImg != VK_NULL_HANDLE)
    {
        vkDestroyImage(mDevice, mShadowDepthImg, nullptr);
        mShadowDepthImg = VK_NULL_HANDLE;
    }
    if (mShadowDepthMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mShadowDepthMem, nullptr);
        mShadowDepthMem = VK_NULL_HANDLE;
    }

    mShadowExtent = { 0, 0 };
    mShadowDepthFormat = VK_FORMAT_UNDEFINED;
}

//--------------------------------------------------------------
// Light VP compute (single directional)
//--------------------------------------------------------------
void VKRenderer::UpdateShadowLightMatrices()
{
    // Build a stable light view/proj based on current camera + directional light
    Vector3 target = GetCameraPosition();

    DirectionalLight dir{};
    Vector3 ambient(0.2f, 0.2f, 0.2f);

    if (auto lm = GetLightingManager())
    {
        dir = lm->GetDirectionalLight();
        ambient = lm->GetAmbientColor();
    }

    Vector3 lightDir = dir.GetDirection();
    if (lightDir.LengthSq() < 1.0e-6f)
    {
        lightDir = Vector3(0.0f, -1.0f, 0.0f);
    }
    lightDir.Normalize();

    // Place light back along direction
    const float dist = std::max(1.0f, mShadowFar * 0.5f);
    Vector3 lightPos = target - lightDir * dist;

    // Up vector: avoid parallel
    Vector3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(Vector3::Dot(up, lightDir)) > 0.98f)
    {
        up = Vector3(1.0f, 0.0f, 0.0f);
    }

    // Ortho extents from settings
    const float w = std::max(1.0f, mShadowOrthoWidth);
    const float h = std::max(1.0f, mShadowOrthoHeight);
    const float zn = std::max(0.01f, mShadowNear);
    const float zf = std::max(zn + 0.01f, mShadowFar);

    // IMPORTANT: Use your Matrix4 utilities consistent with row-vector math.
    // Here we assume you have:
    //  - Matrix4::CreateLookAt(pos, target, up)
    //  - Matrix4::CreateOrtho(width, height, near, far)
    // If your API differs, adjust here only.
    mShadowLightView = Matrix4::CreateLookAt(lightPos, target, up);
    mShadowLightProj = Matrix4::CreateOrtho(w, h, zn, zf);

    // Row-vector: position * World * View * Proj.
    // So LightVP used like: worldPos * LightView * LightProj.
    mShadowLightViewProj = mShadowLightView * mShadowLightProj;

    // Bias matrix to map NDC [-1,1] to UV [0,1]
    // Row-vector variant:
    //   [0.5 0   0   0]
    //   [0   0.5 0   0]
    //   [0   0   1   0]
    //   [0.5 0.5 0   1]
    // (If your Matrix4 is row-major storing, just fill accordingly.)
    mShadowBias = Matrix4::Identity;
    mShadowBias.mat[0][0] = 0.5f;
    mShadowBias.mat[1][1] = 0.5f;
    mShadowBias.mat[2][2] = 1.0f;
    mShadowBias.mat[3][0] = 0.5f;
    mShadowBias.mat[3][1] = 0.5f;
    mShadowBias.mat[3][3] = 1.0f;

    // Final matrix often used in main shading:
    // shadowUV = worldPos * LightVP * Bias
    mShadowLightViewProj_Biased = mShadowLightViewProj * mShadowBias;
}

//--------------------------------------------------------------
// Shadow pass execution
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

    // Ensure resources exist
    if (!CreateShadowResources())
    {
        return;
    }

    // Update light matrices once per frame (world camera already current)
    UpdateShadowLightMatrices();

    // NOTE:
    // ここでは「Shadow専用Pipeline」を使う想定。
    // まだ用意してない段階なら、まずは ShadowPass をスキップしてOK。
    VKPipeline* pipeMesh   = mPipelines.Get("Shadow_Mesh");
    VKPipeline* pipeSkin   = mPipelines.Get("Shadow_SkinnedMesh");

    if ((!pipeMesh || !pipeMesh->IsValid()) && (!pipeSkin || !pipeSkin->IsValid()))
    {
        // Shadow pipeline未整備なら何もしない（クラッシュ回避）
        return;
    }

    VkClearValue clear{};
    clear.depthStencil.depth   = 1.0f;
    clear.depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp{};
    rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass  = mShadowRenderPass;
    rp.framebuffer = mShadowFB;
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

    // TODO:
    // ここで shadow caster bucket を描く。
    // 最短は worldOpaque + skinned を描けばOK。
    //
    // 既存の DrawBucket_World() をそのまま呼ぶと、
    // 既存パイプラインに流れてしまうので、
    // Shadow専用の DrawBucket_Shadow(...) を用意するか、
    // RenderPass 引数で分岐できるようにするのが安全。
    //
    // 今回は「VKRenderer_Shadow.cpp を増やす」だけなので、
    // 呼び出し側（VKRenderer_Drawpass.cpp）で
    // 影用のbucket描画関数を追加してつなぐのがおすすめ。

    vkCmdEndRenderPass(cmd);

    // After pass, we'll transition in RestoreAfterShadowPass()
}

void VKRenderer::RestoreAfterShadowPass()
{
    if (!mDevice || mShadowDepthImg == VK_NULL_HANDLE)
    {
        return;
    }

    VkCommandBuffer cmd = mFrames.empty() ? VK_NULL_HANDLE : mFrames[mFrameIndex].cmd;
    if (!cmd)
    {
        return;
    }

    // Depth attachment -> shader read
    toy::vkutil::CmdTransitionImageLayout(
        cmd,
        mShadowDepthImg,
        DepthAspect(mShadowDepthFormat),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
}

} // namespace toy
