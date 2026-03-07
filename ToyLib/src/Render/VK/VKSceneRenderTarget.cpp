//======================================================================
// VKSceneRenderTarget.cpp
//======================================================================
#include "Render/VK/VKSceneRenderTarget.h"

#include "Render/RenderBackendState.h"
#include "Asset/Material/Texture.h"

#include <iostream>
#include <vector>
#include <algorithm>

namespace toy {

VKSceneRenderTarget::~VKSceneRenderTarget()
{
    Unload();
}

//------------------------------------------------------------------------------
// Bind/Unbind (VK: no-op)
//------------------------------------------------------------------------------
void VKSceneRenderTarget::Bind()
{
    // Vulkanは “現在のcmd” が必要になるので、
    // IRenderTargetのBind/Unbindだけでは開始できない。
    // 実際の BeginRenderPass は VKRenderer::DrawToRenderTarget 側で行う。
}

void VKSceneRenderTarget::Unbind()
{
    // no-op
}

//------------------------------------------------------------------------------
// Resize
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::Resize(int w, int h)
{
    if (w == mW && h == mH)
    {
        return true;
    }
    return Create(w, h);
}

//------------------------------------------------------------------------------
// GetExtent
//------------------------------------------------------------------------------
VkExtent2D VKSceneRenderTarget::GetExtent() const
{
    VkExtent2D e{};
    e.width  = (mW > 0) ? static_cast<uint32_t>(mW) : 0;
    e.height = (mH > 0) ? static_cast<uint32_t>(mH) : 0;
    return e;
}

//------------------------------------------------------------------------------
// Create
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::Create(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        std::cerr << "[VKSceneRenderTarget] Create failed: invalid size "
                  << w << "x" << h << "\n";
        return false;
    }

    // backend handles
    mPhysicalDevice = (VkPhysicalDevice)RenderBackendState::Get().GetVKPhysicalDevice();
    mDevice         = (VkDevice)RenderBackendState::Get().GetVKDevice();

    if (mPhysicalDevice == VK_NULL_HANDLE || mDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] Create failed: device/physical is null.\n";
        return false;
    }

    // recreate safe
    Unload();

    mW = w;
    mH = h;

    mDepthFormat = ChooseDepthFormat();
    if (mDepthFormat == VK_FORMAT_UNDEFINED)
    {
        std::cerr << "[VKSceneRenderTarget] ChooseDepthFormat failed.\n";
        Unload();
        return false;
    }

    if (!CreateImages())
    {
        std::cerr << "[VKSceneRenderTarget] CreateImages failed.\n";
        Unload();
        return false;
    }

    if (!CreateColorSampler())
    {
        std::cerr << "[VKSceneRenderTarget] CreateColorSampler failed.\n";
        Unload();
        return false;
    }

    if (!CreateRenderPass())
    {
        std::cerr << "[VKSceneRenderTarget] CreateRenderPass failed.\n";
        Unload();
        return false;
    }

    if (!CreateFramebuffer())
    {
        std::cerr << "[VKSceneRenderTarget] CreateFramebuffer failed.\n";
        Unload();
        return false;
    }

    if (!CreateWrappedColorTexture())
    {
        std::cerr << "[VKSceneRenderTarget] CreateWrappedColorTexture failed.\n";
        Unload();
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Unload
//------------------------------------------------------------------------------
void VKSceneRenderTarget::Unload()
{
    // Texture wrapper は RT 自身の image/view/sampler を destroy しない
    mColorTex.reset();


    // NOTE: 呼び出し側は vkDeviceWaitIdle 済みが理想（Renderer::Shutdown/WaitIdle など）
    if (mDevice == VK_NULL_HANDLE)
    {
        // device が先に死んでる場合は何もしない
        mW = 0;
        mH = 0;
        return;
    }

    if (mFramebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(mDevice, mFramebuffer, nullptr);
        mFramebuffer = VK_NULL_HANDLE;
    }

    if (mRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    if (mColorSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(mDevice, mColorSampler, nullptr);
        mColorSampler = VK_NULL_HANDLE;
    }

    // views
    if (mColorView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mColorView, nullptr);
        mColorView = VK_NULL_HANDLE;
    }
    if (mDepthView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mDepthView, nullptr);
        mDepthView = VK_NULL_HANDLE;
    }

    // images + memory
    if (mColorImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(mDevice, mColorImage, nullptr);
        mColorImage = VK_NULL_HANDLE;
    }
    if (mColorMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mColorMem, nullptr);
        mColorMem = VK_NULL_HANDLE;
    }

    if (mDepthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(mDevice, mDepthImage, nullptr);
        mDepthImage = VK_NULL_HANDLE;
    }
    if (mDepthMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mDepthMem, nullptr);
        mDepthMem = VK_NULL_HANDLE;
    }

    mW = 0;
    mH = 0;
}

//------------------------------------------------------------------------------
// CreateImages
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::CreateImages()
{
    const VkImageUsageFlags colorUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (!CreateImage2D(
            mW, mH,
            mColorFormat,
            colorUsage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            mColorImage,
            mColorMem,
            mColorView))
    {
        return false;
    }

    const VkImageUsageFlags depthUsage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (mDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
        mDepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (!CreateImage2D(
            mW, mH,
            mDepthFormat,
            depthUsage,
            depthAspect,
            mDepthImage,
            mDepthMem,
            mDepthView))
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateColorSampler
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::CreateColorSampler()
{
    VkSamplerCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter    = VK_FILTER_LINEAR;
    ci.minFilter    = VK_FILTER_LINEAR;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    ci.minLod       = 0.0f;
    ci.maxLod       = 1.0f;
    ci.mipLodBias   = 0.0f;
    ci.anisotropyEnable = VK_FALSE;
    ci.maxAnisotropy    = 1.0f;
    ci.compareEnable    = VK_FALSE;
    ci.compareOp        = VK_COMPARE_OP_ALWAYS;
    ci.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    ci.unnormalizedCoordinates = VK_FALSE;

    const VkResult vr = vkCreateSampler(mDevice, &ci, nullptr, &mColorSampler);
    if (vr != VK_SUCCESS || mColorSampler == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] vkCreateSampler failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateWrappedColorTexture
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::CreateWrappedColorTexture()
{
    mColorTex = std::make_shared<Texture>();
    if (!mColorTex)
    {
        return false;
    }

    if (!mColorTex->WrapVKRenderTarget(
            (void*)mDevice,
            (void*)mColorImage,
            (void*)mColorView,
            (void*)mColorSampler,
            mW,
            mH))
    {
        mColorTex.reset();
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateRenderPass
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::CreateRenderPass()
{
    // Color attachment
    VkAttachmentDescription color{};
    color.format         = mColorFormat;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    // ★ポスト/Surfaceでサンプルするので
    color.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depth{};
    depth.format         = mDepthFormat;
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Dependencies
    VkSubpassDependency deps[2]{};

    // External -> subpass
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // subpass -> External (for sampling)
    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkAttachmentDescription atts[2] = { color, depth };

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 2;
    rpci.pAttachments    = atts;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 2;
    rpci.pDependencies   = deps;

    const VkResult vr = vkCreateRenderPass(mDevice, &rpci, nullptr, &mRenderPass);
    if (vr != VK_SUCCESS || mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] vkCreateRenderPass failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateFramebuffer
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::CreateFramebuffer()
{
    VkImageView attachments[2] = { mColorView, mDepthView };

    VkFramebufferCreateInfo fci{};
    fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass      = mRenderPass;
    fci.attachmentCount = 2;
    fci.pAttachments    = attachments;
    fci.width           = static_cast<uint32_t>(mW);
    fci.height          = static_cast<uint32_t>(mH);
    fci.layers          = 1;

    const VkResult vr = vkCreateFramebuffer(mDevice, &fci, nullptr, &mFramebuffer);
    if (vr != VK_SUCCESS || mFramebuffer == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] vkCreateFramebuffer failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateImage2D
//------------------------------------------------------------------------------
bool VKSceneRenderTarget::CreateImage2D(
    int w,
    int h,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkImage& outImage,
    VkDeviceMemory& outMem,
    VkImageView& outView)
{
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = format;
    ici.extent.width  = static_cast<uint32_t>(w);
    ici.extent.height = static_cast<uint32_t>(h);
    ici.extent.depth  = 1;
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = usage;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult vr = vkCreateImage(mDevice, &ici, nullptr, &outImage);
    if (vr != VK_SUCCESS || outImage == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] vkCreateImage failed: " << vr << "\n";
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(mDevice, outImage, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (mai.memoryTypeIndex == UINT32_MAX)
    {
        std::cerr << "[VKSceneRenderTarget] FindMemoryType failed.\n";
        return false;
    }

    vr = vkAllocateMemory(mDevice, &mai, nullptr, &outMem);
    if (vr != VK_SUCCESS || outMem == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] vkAllocateMemory failed: " << vr << "\n";
        return false;
    }

    vr = vkBindImageMemory(mDevice, outImage, outMem, 0);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKSceneRenderTarget] vkBindImageMemory failed: " << vr << "\n";
        return false;
    }

    VkImageViewCreateInfo ivci{};
    ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image    = outImage;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format   = format;

    ivci.subresourceRange.aspectMask     = aspect;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;

    vr = vkCreateImageView(mDevice, &ivci, nullptr, &outView);
    if (vr != VK_SUCCESS || outView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKSceneRenderTarget] vkCreateImageView failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// FindMemoryType
//------------------------------------------------------------------------------
uint32_t VKSceneRenderTarget::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    {
        const bool typeOk = (typeBits & (1u << i)) != 0;
        const bool propOk = (mp.memoryTypes[i].propertyFlags & props) == props;
        if (typeOk && propOk)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

//------------------------------------------------------------------------------
// ChooseDepthFormat
//------------------------------------------------------------------------------
VkFormat VKSceneRenderTarget::ChooseDepthFormat() const
{
    // 優先順（現場で安定しやすい順）
    const VkFormat candidates[] =
    {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    for (VkFormat fmt : candidates)
    {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, fmt, &fp);

        if ((fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            return fmt;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

} // namespace toy
