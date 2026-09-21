//======================================================================
// VKPostMidTarget.h
//  - PostEffect 2段目用の中間オフスクリーンターゲット(color only, depth無し)
//  - Owns: VkImage/VkImageView(color) + RenderPass + Framebuffer
//  - finalLayout: SHADER_READ_ONLY_OPTIMAL (次段のPostEffectでサンプルする)
//  - RenderPassはサイズに依存しないため一度作ったら使い回し、
//    image/view/framebufferだけをリサイズ時に作り直す
//    (VKSceneRenderTargetと違い、深度アタッチメントを持たない分軽量)。
//======================================================================
#pragma once

#include "Render/IRenderTarget.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace toy {

class VKPostMidTarget : public IRenderTarget
{
public:
    ~VKPostMidTarget() override;

    bool Create(int w, int h) override;
    void Unload() override;

    void Bind() override;   // VKでは no-op(RendererがcmdでBegin/Endする)
    void Unbind() override; // VKでは no-op

    bool Resize(int w, int h) override;

    VkExtent2D    GetExtent() const;
    VkRenderPass  GetRenderPass() const { return mRenderPass; }
    VkFramebuffer GetFramebuffer() const { return mFramebuffer; }

private:
    bool EnsureRenderPass();
    bool CreateImage();
    void TransitionColorToShaderReadOnly();
    bool CreateColorSampler();
    bool CreateFramebuffer();
    bool CreateWrappedColorTexture();

    void DestroySizedResources();

    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

private:
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    VkFormat mColorFormat = VK_FORMAT_B8G8R8A8_UNORM;

    // Color (サイズ依存、リサイズ毎に作り直す)
    VkImage        mColorImage   = VK_NULL_HANDLE;
    VkDeviceMemory mColorMem     = VK_NULL_HANDLE;
    VkImageView    mColorView    = VK_NULL_HANDLE;
    VkFramebuffer  mFramebuffer  = VK_NULL_HANDLE;

    // サイズに依存しないので使い回す
    VkSampler      mColorSampler = VK_NULL_HANDLE;
    VkRenderPass   mRenderPass   = VK_NULL_HANDLE;
};

} // namespace toy
