// Render/VK/VKTextureGPU.h
#pragma once

#include "Render/ITextureGPU.h"
#include <vulkan/vulkan.h>

namespace toy {

//============================================================
// VKTextureGPU
//  - Vulkan 実装（最小：RGBA8 テクスチャを作って貼れる）
//  - CreateShadowMap / CreateRenderColorRGBA8 は後で拡張
//============================================================
class VKTextureGPU final : public ITextureGPU
{
public:
    VKTextureGPU();
    ~VKTextureGPU() override;

    bool CreateFromPixels(const void* pixels,
                          int width,
                          int height,
                          bool hasAlpha) override;

    void CreateShadowMap(int width, int height) override;
    void CreateRenderColorRGBA8(int w, int h) override;

    // GL の "SetActive" 相当は VK では基本 no-op（descriptor を Bind するので）
    void SetActive(int unit) override;

    void Unload() override;

    // VKRenderer 側から参照するための getter（Texture の public API は増やさない）
    VkImageView GetImageView() const { return mView; }
    VkSampler   GetSampler()  const { return mSampler; }
    VkImage     GetImage()    const { return mImage; }

    int GetWidth()  const { return mWidth; }
    int GetHeight() const { return mHeight; }

private:
    // Context (non-owning)
    VkPhysicalDevice mPhysicalDevice { VK_NULL_HANDLE };
    VkDevice         mDevice { VK_NULL_HANDLE };
    VkQueue          mGraphicsQueue { VK_NULL_HANDLE };
    VkCommandPool    mCommandPool { VK_NULL_HANDLE };

    // Resource
    VkImage        mImage { VK_NULL_HANDLE };
    VkDeviceMemory mMemory { VK_NULL_HANDLE };
    VkImageView    mView { VK_NULL_HANDLE };
    VkSampler      mSampler { VK_NULL_HANDLE };

    int mWidth  { 0 };
    int mHeight { 0 };

private:
    bool CreateImage(int width, int height, VkFormat format,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags memFlags);

    bool CreateImageView(VkFormat format);
    bool CreateSampler();

    bool UploadRGBA8(const void* pixels, int width, int height);

    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags memFlags,
                      VkBuffer& outBuf, VkDeviceMemory& outMem);

    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    bool BeginOneShot(VkCommandBuffer& outCmd);
    void EndOneShot(VkCommandBuffer cmd);

    void TransitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout);
    void CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image,
                           uint32_t width, uint32_t height);
};

} // namespace toy
