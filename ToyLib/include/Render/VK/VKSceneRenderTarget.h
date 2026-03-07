//======================================================================
// VKSceneRenderTarget.h
//  - Vulkan RTT target (color + depth)
//  - Owns: VkImage/VkImageView (color/depth) + RenderPass + Framebuffer
//  - finalLayout of color: SHADER_READ_ONLY_OPTIMAL (for post sampling)
//======================================================================
#pragma once

#include "Render/IRenderTarget.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace toy {

//==============================================================
// VKSceneRenderTarget
//==============================================================
class VKSceneRenderTarget : public IRenderTarget
{
public:
    ~VKSceneRenderTarget() override;

    bool Create(int w, int h) override;
    void Unload() override;

    void Bind() override;   // VKでは no-op（Rendererがcmdを握ってBegin/Endする）
    void Unbind() override; // VKでは no-op

    bool Resize(int w, int h) override;

    //--------------------------------------------------------------------------
    // VK native getters (Renderer側が使う)
    //--------------------------------------------------------------------------
    VkExtent2D GetExtent() const;

    VkFormat   GetColorFormat() const { return mColorFormat; }
    VkFormat   GetDepthFormat() const { return mDepthFormat; }

    VkImageView GetColorView() const { return mColorView; }
    VkImageView GetDepthView() const { return mDepthView; }
    VkSampler   GetColorSampler() const { return mColorSampler; }

    VkRenderPass  GetRenderPass() const { return mRenderPass; }
    VkFramebuffer GetFramebuffer() const { return mFramebuffer; }

private:
    bool CreateImages();
    bool CreateRenderPass();
    bool CreateFramebuffer();

    bool CreateColorSampler();
    bool CreateWrappedColorTexture();

    bool CreateImage2D(
        int w,
        int h,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspect,
        VkImage& outImage,
        VkDeviceMemory& outMem,
        VkImageView& outView);

    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    VkFormat ChooseDepthFormat() const;

private:
    // cached backend handles (from RenderBackendState)
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    // formats
    VkFormat mColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED;

    // Color
    VkImage        mColorImage   = VK_NULL_HANDLE;
    VkDeviceMemory mColorMem     = VK_NULL_HANDLE;
    VkImageView    mColorView    = VK_NULL_HANDLE;
    VkSampler      mColorSampler = VK_NULL_HANDLE;

    // Depth
    VkImage        mDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory mDepthMem   = VK_NULL_HANDLE;
    VkImageView    mDepthView  = VK_NULL_HANDLE;

    // Pass/FB
    VkRenderPass   mRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer  mFramebuffer = VK_NULL_HANDLE;
};

} // namespace toy
