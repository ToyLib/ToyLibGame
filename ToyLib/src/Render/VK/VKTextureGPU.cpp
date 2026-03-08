// Render/VK/VKTextureGPU.cpp
#include "Render/VK/VKTextureGPU.h"
#include "Render/RenderBackendState.h"

#include <cstring>
#include <iostream>
#include <vector>

namespace toy {

static VkPhysicalDevice GetVKPhysicalDevice()
{
    return (VkPhysicalDevice)RenderBackendState::Get().GetVKPhysicalDevice();
}

static VkDevice GetVKDevice()
{
    return (VkDevice)RenderBackendState::Get().GetVKDevice();
}

static VkQueue GetVKGraphicsQueue()
{
    return (VkQueue)RenderBackendState::Get().GetVKGraphicsQueue();
}

static VkCommandPool GetVKCommandPool()
{
    return (VkCommandPool)RenderBackendState::Get().GetVKCommandPool();
}

VKTextureGPU::VKTextureGPU()
{
    mPhysicalDevice = GetVKPhysicalDevice();
    mDevice         = GetVKDevice();
    mGraphicsQueue  = GetVKGraphicsQueue();
    mCommandPool    = GetVKCommandPool();
}

VKTextureGPU::~VKTextureGPU()
{
    Unload();
}

void VKTextureGPU::Unload()
{
    if (!mDevice) return;

    if (mOwnsImage)
    {
        if (mSampler)
        {
            vkDestroySampler(mDevice, mSampler, nullptr);
            mSampler = VK_NULL_HANDLE;
        }
        if (mView)
        {
            vkDestroyImageView(mDevice, mView, nullptr);
            mView = VK_NULL_HANDLE;
        }
        if (mImage)
        {
            vkDestroyImage(mDevice, mImage, nullptr);
            mImage = VK_NULL_HANDLE;
        }
        if (mMemory)
        {
            vkFreeMemory(mDevice, mMemory, nullptr);
            mMemory = VK_NULL_HANDLE;
        }
    }
    else
    {
        mSampler = VK_NULL_HANDLE;
        mView    = VK_NULL_HANDLE;
        mImage   = VK_NULL_HANDLE;
        mMemory  = VK_NULL_HANDLE;
    }

    mWidth = 0;
    mHeight = 0;
    mOwnsImage = true;
}

void VKTextureGPU::SetActive(int /*unit*/)
{
    // Vulkan では descriptor に image/sampler を詰めて bind するので no-op でOK
}

bool VKTextureGPU::CreateFromPixels(const void* pixels, int width, int height, bool hasAlpha)
{
    if (!mDevice || !mPhysicalDevice || !mGraphicsQueue || !mCommandPool)
    {
        std::cerr << "[VKTextureGPU] context is not set.\n";
        return false;
    }
    if (!pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    Unload();

    mWidth = width;
    mHeight = height;

    // 内部は常にRGBA8
    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    if (!CreateImage(width, height, format,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
        return false;
    }

    // 入力をRGBA8に正規化してからアップロード
    std::vector<uint8_t> rgba;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(pixels);

    if (hasAlpha)
    {
        // 入力がRGBA8前提（4byte/pixel）
        // ※ここが本当にRGBAか怪しい場合は “bytesPerPixel/format” を上位から渡す必要あり
        rgba.assign(src, src + (size_t)width * (size_t)height * 4);
    }
    else
    {
        // 入力がRGB8(3byte/pixel)前提 → RGBA8へ展開（A=255）
        rgba.resize((size_t)width * (size_t)height * 4);

        const size_t n = (size_t)width * (size_t)height;
        for (size_t i = 0; i < n; ++i)
        {
            rgba[i * 4 + 0] = src[i * 3 + 0];
            rgba[i * 4 + 1] = src[i * 3 + 1];
            rgba[i * 4 + 2] = src[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
    }

    if (!UploadRGBA8(rgba.data(), width, height))
    {
        return false;
    }

    if (!CreateImageView(format))
    {
        return false;
    }

    if (!CreateSampler())
    {
        return false;
    }

    return true;
}

void VKTextureGPU::CreateShadowMap(int /*width*/, int /*height*/)
{
    // ここは後で：depth image + view + sampler(比較) など
    // 今は sprite 目的なので未実装でOK
}

void VKTextureGPU::CreateRenderColorRGBA8(int /*w*/, int /*h*/)
{
    // ここは後で：RenderTarget 用 image を作る
    // swapchainとは別の offscreen
}

//--------------------------------------------------------------
// helpers
//--------------------------------------------------------------
uint32_t VKTextureGPU::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    {
        const bool typeOk = (typeBits & (1u << i)) != 0;
        const bool propOk = (mp.memoryTypes[i].propertyFlags & props) == props;
        if (typeOk && propOk) return i;
    }
    return UINT32_MAX;
}

bool VKTextureGPU::CreateBuffer(VkDeviceSize size,
                                VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags memFlags,
                                VkBuffer& outBuf,
                                VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(mDevice, outBuf, &req);

    const uint32_t memType = FindMemoryType(req.memoryTypeBits, memFlags);
    if (memType == UINT32_MAX)
    {
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    if (vkAllocateMemory(mDevice, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(mDevice, outBuf, outMem, 0);
    return true;
}

bool VKTextureGPU::CreateImage(int width, int height, VkFormat format,
                               VkImageUsageFlags usage, VkMemoryPropertyFlags memFlags)
{
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width  = (uint32_t)width;
    ici.extent.height = (uint32_t)height;
    ici.extent.depth  = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = format;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = usage;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(mDevice, &ici, nullptr, &mImage) != VK_SUCCESS)
    {
        mImage = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(mDevice, mImage, &req);

    const uint32_t memType = FindMemoryType(req.memoryTypeBits, memFlags);
    if (memType == UINT32_MAX)
    {
        vkDestroyImage(mDevice, mImage, nullptr);
        mImage = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    if (vkAllocateMemory(mDevice, &mai, nullptr, &mMemory) != VK_SUCCESS)
    {
        vkDestroyImage(mDevice, mImage, nullptr);
        mImage = VK_NULL_HANDLE;
        mMemory = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(mDevice, mImage, mMemory, 0);
    return true;
}

bool VKTextureGPU::CreateImageView(VkFormat format)
{
    VkImageViewCreateInfo ivci{};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = mImage;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(mDevice, &ivci, nullptr, &mView) != VK_SUCCESS)
    {
        mView = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool VKTextureGPU::CreateSampler()
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
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    sci.compareEnable = VK_FALSE;
    sci.compareOp = VK_COMPARE_OP_ALWAYS;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;

    if (vkCreateSampler(mDevice, &sci, nullptr, &mSampler) != VK_SUCCESS)
    {
        mSampler = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool VKTextureGPU::BeginOneShot(VkCommandBuffer& outCmd)
{
    outCmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = mCommandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(mDevice, &ai, &outCmd) != VK_SUCCESS)
    {
        outCmd = VK_NULL_HANDLE;
        return false;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(outCmd, &bi) != VK_SUCCESS)
    {
        vkFreeCommandBuffers(mDevice, mCommandPool, 1, &outCmd);
        outCmd = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VKTextureGPU::EndOneShot(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkQueueSubmit(mGraphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
}

void VKTextureGPU::TransitionImageLayout(VkCommandBuffer cmd,
                                         VkImage image,
                                         VkImageLayout oldLayout,
                                         VkImageLayout newLayout)
{
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd,
                         srcStage, dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &b);
}

void VKTextureGPU::CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image,
                                     uint32_t width, uint32_t height)
{
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd,
                           buffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
}

bool VKTextureGPU::UploadRGBA8(const void* pixels, int width, int height)
{
    const VkDeviceSize size = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (!CreateBuffer(size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging,
                      stagingMem))
    {
        return false;
    }

    void* mapped = nullptr;
    vkMapMemory(mDevice, stagingMem, 0, size, 0, &mapped);
    std::memcpy(mapped, pixels, (size_t)size);
    vkUnmapMemory(mDevice, stagingMem);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!BeginOneShot(cmd))
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        return false;
    }

    TransitionImageLayout(cmd, mImage,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(cmd, staging, mImage, (uint32_t)width, (uint32_t)height);

    TransitionImageLayout(cmd, mImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndOneShot(cmd);

    vkDestroyBuffer(mDevice, staging, nullptr);
    vkFreeMemory(mDevice, stagingMem, nullptr);

    return true;
}

bool VKTextureGPU::WrapExternalRenderTarget(VkDevice device,
                                            VkImage image,
                                            VkImageView view,
                                            VkSampler sampler,
                                            int width,
                                            int height)
{
    Unload();

    if (device == VK_NULL_HANDLE ||
        image == VK_NULL_HANDLE ||
        view == VK_NULL_HANDLE ||
        sampler == VK_NULL_HANDLE ||
        width <= 0 || height <= 0)
    {
        return false;
    }

    mDevice    = device;
    mImage     = image;
    mView      = view;
    mSampler   = sampler;
    mWidth     = width;
    mHeight    = height;
    mOwnsImage = false;

    return true;
}

} // namespace toy
