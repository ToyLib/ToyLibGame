//======================================================================
// VKPostMidTarget.cpp
//======================================================================
#include "Render/VK/VKPostMidTarget.h"

#include "Render/RenderBackendState.h"
#include "Render/VK/VKUtil.h"
#include "Asset/Material/Texture.h"

#include <iostream>

namespace toy {

VKPostMidTarget::~VKPostMidTarget()
{
    Unload();
}

//------------------------------------------------------------------------------
// Bind/Unbind (VK: no-op)
//------------------------------------------------------------------------------
void VKPostMidTarget::Bind()
{
    // 実際の BeginRenderPass は VKRenderer::DrawPostEffectPass 側で行う。
}

void VKPostMidTarget::Unbind()
{
    // no-op
}

//------------------------------------------------------------------------------
// Resize
//------------------------------------------------------------------------------
bool VKPostMidTarget::Resize(int w, int h)
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
VkExtent2D VKPostMidTarget::GetExtent() const
{
    VkExtent2D e{};
    e.width  = (mW > 0) ? static_cast<uint32_t>(mW) : 0;
    e.height = (mH > 0) ? static_cast<uint32_t>(mH) : 0;
    return e;
}

//------------------------------------------------------------------------------
// Create
//------------------------------------------------------------------------------
bool VKPostMidTarget::Create(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        std::cerr << "[VKPostMidTarget] Create failed: invalid size " << w << "x" << h << "\n";
        return false;
    }

    mPhysicalDevice = (VkPhysicalDevice)RenderBackendState::Get().GetVKPhysicalDevice();
    mDevice         = (VkDevice)RenderBackendState::Get().GetVKDevice();

    if (mPhysicalDevice == VK_NULL_HANDLE || mDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPostMidTarget] Create failed: device/physical is null.\n";
        return false;
    }

    // サイズ依存分だけ作り直す(RenderPass/Samplerは使い回す)
    DestroySizedResources();

    mW = w;
    mH = h;

    if (!EnsureRenderPass())
    {
        std::cerr << "[VKPostMidTarget] EnsureRenderPass failed.\n";
        Unload();
        return false;
    }

    if (!CreateImage())
    {
        std::cerr << "[VKPostMidTarget] CreateImage failed.\n";
        Unload();
        return false;
    }

    // ★colorをfinalLayout(SHADER_READ_ONLY_OPTIMAL)へ先に遷移しておく。
    //   VKSceneRenderTargetと同じ理由(初回フレームでUNDEFINEDのままサンプルされるのを防ぐ)。
    TransitionColorToShaderReadOnly();

    if (!CreateColorSampler())
    {
        std::cerr << "[VKPostMidTarget] CreateColorSampler failed.\n";
        Unload();
        return false;
    }

    if (!CreateFramebuffer())
    {
        std::cerr << "[VKPostMidTarget] CreateFramebuffer failed.\n";
        Unload();
        return false;
    }

    if (!CreateWrappedColorTexture())
    {
        std::cerr << "[VKPostMidTarget] CreateWrappedColorTexture failed.\n";
        Unload();
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Unload
//------------------------------------------------------------------------------
void VKPostMidTarget::Unload()
{
    // Texture wrapper は RT 自身の image/view/sampler を destroy しない
    mColorTex.reset();

    if (mDevice == VK_NULL_HANDLE)
    {
        // device が先に死んでる場合は何もしない
        mW = 0;
        mH = 0;
        return;
    }

    DestroySizedResources();

    if (mColorSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(mDevice, mColorSampler, nullptr);
        mColorSampler = VK_NULL_HANDLE;
    }

    if (mRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    mW = 0;
    mH = 0;
}

//------------------------------------------------------------------------------
// DestroySizedResources
//  - サイズに依存するリソース(framebuffer/image/view/mem)だけを破棄する。
//    RenderPass/Samplerはサイズ非依存なので使い回す。
//------------------------------------------------------------------------------
void VKPostMidTarget::DestroySizedResources()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return;
    }

    if (mFramebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(mDevice, mFramebuffer, nullptr);
        mFramebuffer = VK_NULL_HANDLE;
    }

    if (mColorView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mColorView, nullptr);
        mColorView = VK_NULL_HANDLE;
    }

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
}

//------------------------------------------------------------------------------
// EnsureRenderPass
//  - サイズに依存しないので一度作ったら使い回す
//------------------------------------------------------------------------------
bool VKPostMidTarget::EnsureRenderPass()
{
    if (mRenderPass != VK_NULL_HANDLE)
    {
        return true;
    }

    VkAttachmentDescription color{};
    color.format         = mColorFormat;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    // ★次段のPostEffectでサンプルするので
    color.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;
    subpass.pDepthStencilAttachment = nullptr;

    // 前フレームでこの画像を「サンプルし終わる」まで、今フレームの書き込み開始を待つ
    // (write-after-readハザード対策。VKSceneRenderTargetと同種の同期)
    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &color;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    const VkResult vr = vkCreateRenderPass(mDevice, &rpci, nullptr, &mRenderPass);
    if (vr != VK_SUCCESS || mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPostMidTarget] vkCreateRenderPass failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateImage
//------------------------------------------------------------------------------
bool VKPostMidTarget::CreateImage()
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = mColorFormat;
    ici.extent.width  = static_cast<uint32_t>(mW);
    ici.extent.height = static_cast<uint32_t>(mH);
    ici.extent.depth  = 1;
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = usage;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult vr = vkCreateImage(mDevice, &ici, nullptr, &mColorImage);
    if (vr != VK_SUCCESS || mColorImage == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPostMidTarget] vkCreateImage failed: " << vr << "\n";
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(mDevice, mColorImage, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (mai.memoryTypeIndex == UINT32_MAX)
    {
        std::cerr << "[VKPostMidTarget] FindMemoryType failed.\n";
        return false;
    }

    vr = vkAllocateMemory(mDevice, &mai, nullptr, &mColorMem);
    if (vr != VK_SUCCESS || mColorMem == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPostMidTarget] vkAllocateMemory failed: " << vr << "\n";
        return false;
    }

    vr = vkBindImageMemory(mDevice, mColorImage, mColorMem, 0);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKPostMidTarget] vkBindImageMemory failed: " << vr << "\n";
        return false;
    }

    VkImageViewCreateInfo ivci{};
    ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image    = mColorImage;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format   = mColorFormat;

    ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;

    vr = vkCreateImageView(mDevice, &ivci, nullptr, &mColorView);
    if (vr != VK_SUCCESS || mColorView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPostMidTarget] vkCreateImageView failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// TransitionColorToShaderReadOnly
//------------------------------------------------------------------------------
void VKPostMidTarget::TransitionColorToShaderReadOnly()
{
    if (mColorImage == VK_NULL_HANDLE)
    {
        return;
    }

    auto commandPool = static_cast<VkCommandPool>(RenderBackendState::Get().GetVKCommandPool());
    auto queue = static_cast<VkQueue>(RenderBackendState::Get().GetVKGraphicsQueue());
    if (commandPool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE)
    {
        return;
    }

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(mDevice, &ai, &cmd) != VK_SUCCESS || cmd == VK_NULL_HANDLE)
    {
        return;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    toy::vkutil::CmdTransitionImageLayout(cmd, mColorImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, VK_ACCESS_SHADER_READ_BIT);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(mDevice, commandPool, 1, &cmd);
}

//------------------------------------------------------------------------------
// CreateColorSampler
//  - サイズ非依存なので一度作ったら使い回す
//------------------------------------------------------------------------------
bool VKPostMidTarget::CreateColorSampler()
{
    if (mColorSampler != VK_NULL_HANDLE)
    {
        return true;
    }

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
        std::cerr << "[VKPostMidTarget] vkCreateSampler failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateFramebuffer
//------------------------------------------------------------------------------
bool VKPostMidTarget::CreateFramebuffer()
{
    VkFramebufferCreateInfo fci{};
    fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass      = mRenderPass;
    fci.attachmentCount = 1;
    fci.pAttachments    = &mColorView;
    fci.width           = static_cast<uint32_t>(mW);
    fci.height          = static_cast<uint32_t>(mH);
    fci.layers          = 1;

    const VkResult vr = vkCreateFramebuffer(mDevice, &fci, nullptr, &mFramebuffer);
    if (vr != VK_SUCCESS || mFramebuffer == VK_NULL_HANDLE)
    {
        std::cerr << "[VKPostMidTarget] vkCreateFramebuffer failed: " << vr << "\n";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// CreateWrappedColorTexture
//------------------------------------------------------------------------------
bool VKPostMidTarget::CreateWrappedColorTexture()
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
// FindMemoryType
//------------------------------------------------------------------------------
uint32_t VKPostMidTarget::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
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

} // namespace toy
