// Render/VK/VKRenderer_DummyWhite.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKUtil.h"

#include <array>
#include <iostream>
#include <cstring>

namespace toy
{

bool VKRenderer::CreateDummyWhiteResources()
{
    if (mDummyWhiteImageView != VK_NULL_HANDLE && mDummyWhiteSampler != VK_NULL_HANDLE)
    {
        return true;
    }

    // 1x1 white RGBA
    const uint32_t whitePixel = 0xFFFFFFFFu;

    // staging buffer
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (!CreateBuffer(sizeof(uint32_t),
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging, stagingMem))
    {
        return false;
    }

    void* mapped = nullptr;
    vkMapMemory(mDevice, stagingMem, 0, sizeof(uint32_t), 0, &mapped);
    std::memcpy(mapped, &whitePixel, sizeof(uint32_t));
    vkUnmapMemory(mDevice, stagingMem);

    // image create
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {1, 1, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(mDevice, &ici, nullptr, &mDummyWhiteImage) != VK_SUCCESS)
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        return false;
    }

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(mDevice, mDummyWhiteImage, &mr);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(mDevice, &mai, nullptr, &mDummyWhiteMemory) != VK_SUCCESS)
    {
        vkDestroyImage(mDevice, mDummyWhiteImage, nullptr);
        mDummyWhiteImage = VK_NULL_HANDLE;

        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        return false;
    }

    vkBindImageMemory(mDevice, mDummyWhiteImage, mDummyWhiteMemory, 0);

    // upload
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!BeginOneShot(cmd))
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        return false;
    }

    TransitionImageLayout(cmd, mDummyWhiteImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(cmd, staging, mDummyWhiteImage, 1, 1);
    TransitionImageLayout(cmd, mDummyWhiteImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndOneShot(cmd);

    vkDestroyBuffer(mDevice, staging, nullptr);
    vkFreeMemory(mDevice, stagingMem, nullptr);

    // view
    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = mDummyWhiteImage;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(mDevice, &vci, nullptr, &mDummyWhiteImageView) != VK_SUCCESS)
    {
        return false;
    }

    // sampler
    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxAnisotropy = 1.0f;
    sci.anisotropyEnable = VK_FALSE;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    sci.compareEnable = VK_FALSE;
    sci.compareOp = VK_COMPARE_OP_ALWAYS;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;

    if (vkCreateSampler(mDevice, &sci, nullptr, &mDummyWhiteSampler) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void VKRenderer::DestroyDummyWhiteResources()
{
    if (mDummyWhiteSampler)
    {
        vkDestroySampler(mDevice, mDummyWhiteSampler, nullptr);
        mDummyWhiteSampler = VK_NULL_HANDLE;
    }
    if (mDummyWhiteImageView)
    {
        vkDestroyImageView(mDevice, mDummyWhiteImageView, nullptr);
        mDummyWhiteImageView = VK_NULL_HANDLE;
    }
    if (mDummyWhiteImage)
    {
        vkDestroyImage(mDevice, mDummyWhiteImage, nullptr);
        mDummyWhiteImage = VK_NULL_HANDLE;
    }
    if (mDummyWhiteMemory)
    {
        vkFreeMemory(mDevice, mDummyWhiteMemory, nullptr);
        mDummyWhiteMemory = VK_NULL_HANDLE;
    }

    mWorldTexDescSetDummyWhite = VK_NULL_HANDLE;
}

} // namespace toy
