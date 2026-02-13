//======================================================================
// VKRenderer_UIResources.cpp
//  - 最小 UI リソース（fallback 1x1 white + sampler）
//  - SpriteDescSet の fallback として使う前提
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKUtil.h"

#include <iostream>
#include <vector>
#include <cstring>

namespace toy
{

//--------------------------------------------------------------
// 1x1 白テクを HostVisible(LINEAR) で作る最短版
//  ※正道は staging + OPTIMAL + layout遷移
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
    ici.tiling    = VK_IMAGE_TILING_LINEAR;
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
        vkutil::FindMemoryType(phys, req.memoryTypeBits,
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

    // write white pixel (RGBA8)
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

    uint8_t* ptr = (uint8_t*)mapped;
    ptr[layout.offset + 0] = 255;
    ptr[layout.offset + 1] = 255;
    ptr[layout.offset + 2] = 255;
    ptr[layout.offset + 3] = 255;

    vkUnmapMemory(device, outMem);

    // view
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
// layout transition helper
//--------------------------------------------------------------
static void CmdTransitionToShaderRead(VkCommandBuffer cmd, VkImage img)
{
    VkImageMemoryBarrier bar{};
    bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.image = img;
    bar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bar.subresourceRange.baseMipLevel = 0;
    bar.subresourceRange.levelCount = 1;
    bar.subresourceRange.baseArrayLayer = 0;
    bar.subresourceRange.layerCount = 1;

    bar.srcAccessMask = 0;
    bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &bar);
}

//--------------------------------------------------------------
// EnsureUIResources
//--------------------------------------------------------------
bool VKRenderer::EnsureUIResources()
{
    if (mUiReady)
    {
        return true;
    }

    if (!mDevice || !mPhysicalDevice)
    {
        std::cerr << "[VKRenderer] EnsureUIResources: device/phys missing.\n";
        return false;
    }

    // (1) sampler
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
            std::cerr << "[VKRenderer] EnsureUIResources: vkCreateSampler failed.\n";
            return false;
        }
    }

    // (2) 1x1 white image/view
    if (!CreateImage1x1White_Linear(mPhysicalDevice, mDevice,
                                   mUiTestImage, mUiTestImageMemory, mUiTestImageView))
    {
        std::cerr << "[VKRenderer] EnsureUIResources: CreateImage1x1White_Linear failed.\n";
        return false;
    }

    // (3) one-time layout transition (recorded into current frame cmd)
    //     ※本来は dedicated init cmd/queue を作るが、最短で BeginFrame 後に一回だけ
    mUiImageLayoutReady = false;

    // Sprite fallback としても使う
    mSpriteFallbackImageView = mUiTestImageView;
    mSpriteFallbackSampler   = mUiSampler;

    mUiReady = true;
    return true;
}

//--------------------------------------------------------------
// DestroyUIResources
//--------------------------------------------------------------
void VKRenderer::DestroyUIResources()
{
    if (!mDevice)
    {
        mUiReady = false;
        return;
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

    if (mUiSampler)
    {
        vkDestroySampler(mDevice, mUiSampler, nullptr);
        mUiSampler = VK_NULL_HANDLE;
    }

    // UI test quad buffers（残ってるなら）
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
    mUiImageLayoutReady = false;
    mUiReady = false;

    // Sprite fallback も無効化
    mSpriteFallbackImageView = VK_NULL_HANDLE;
    mSpriteFallbackSampler   = VK_NULL_HANDLE;
}

} // namespace toy
