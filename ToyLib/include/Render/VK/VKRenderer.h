//======================================================================
// VKRenderer.h
//  - SDL3 + Vulkan
//  - Swapchain + Depth (Z enabled)
//  - RTT via VKSceneRenderTarget (IRenderTarget derived)
//======================================================================
#pragma once

#include "Render/IRenderer.h"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace toy
{

class Application;
class IRenderTarget;

//==============================================================================
// VKRenderer
//==============================================================================
class VKRenderer : public IRenderer
{
public:
    VKRenderer();
    ~VKRenderer() override;

    bool Initialize(const Application* app) override;
    void Shutdown() override;
    void WaitIdle() override;

    // RTT
    std::shared_ptr<IRenderTarget> CreateRenderTarget() override;
    void DrawToRenderTarget(const SceneCaptureRequest& req) override;

    PipelineHandle GetPipelineHandle(const std::string& name) override;

    void OnWindowResized(int pixelW, int pixelH) override;

protected:
    // IRenderer draw phases
    bool BeginFrame() override;

    void DrawShadowPass() override;
    void RestoreAfterShadowPass() override;

    void DrawSkyPass() override;
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override;
    void DrawFadePass() override;
    void DrawPostEffectPass() override;
    void DrawUIPass() override;

    void EndFrame() override;

protected:
    // IRenderer::DrawItem override (bucketed draw path)
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

private:
    //--------------------------------------------------------------------------
    // Init steps (core)
    //--------------------------------------------------------------------------
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateDeviceAndQueues();
    bool CreateSwapchainAndViews();

    // Swapchain dependent
    bool CreateDepthForSwapchain();
    bool CreateRenderPass();
    bool CreateFramebuffers();

    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();

    //--------------------------------------------------------------------------
    // Recreate swapchain
    //--------------------------------------------------------------------------
    bool RecreateSwapchain();
    void CleanupSwapchain();

    //--------------------------------------------------------------------------
    // Command helpers
    //--------------------------------------------------------------------------
    VkCommandBuffer BeginOneTimeCommands();
    void EndOneTimeCommands(VkCommandBuffer cmd);

    //--------------------------------------------------------------------------
    // Utilities
    //--------------------------------------------------------------------------
    VkFormat ChooseDepthFormat() const;
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    bool CreateImage2D(
        uint32_t w,
        uint32_t h,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memProps,
        VkImage& outImage,
        VkDeviceMemory& outMem);

    bool CreateImageView2D(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        VkImageView& outView);

private:
    //--------------------------------------------------------------------------
    // Validation
    //--------------------------------------------------------------------------
    bool mEnableValidation { false };

    //--------------------------------------------------------------------------
    // Core Vulkan handles
    //--------------------------------------------------------------------------
    VkInstance   mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface  = VK_NULL_HANDLE;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    VkQueue  mQueueGraphics = VK_NULL_HANDLE;
    VkQueue  mQueuePresent  = VK_NULL_HANDLE;
    uint32_t mQueueFamilyGraphics = UINT32_MAX;
    uint32_t mQueueFamilyPresent  = UINT32_MAX;

    // Debug messenger (optional)
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

    //--------------------------------------------------------------------------
    // Swapchain
    //--------------------------------------------------------------------------
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;

    VkSurfaceFormatKHR     mSwapchainFormat {};
    VkExtent2D             mSwapchainExtent {};
    std::vector<VkImage>   mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    //--------------------------------------------------------------------------
    // Swapchain depth (Z buffer)
    //--------------------------------------------------------------------------
    VkFormat       mDepthFormat = VK_FORMAT_UNDEFINED;
    VkImage        mDepthImage  = VK_NULL_HANDLE;
    VkDeviceMemory mDepthMem    = VK_NULL_HANDLE;
    VkImageView    mDepthView   = VK_NULL_HANDLE;

    //--------------------------------------------------------------------------
    // RenderPass / Framebuffers (swapchain)
    //--------------------------------------------------------------------------
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mFramebuffers;

    //--------------------------------------------------------------------------
    // Commands
    //--------------------------------------------------------------------------
    VkCommandPool mCommandPool = VK_NULL_HANDLE;

    struct FrameSync
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;

        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence     inFlight       = VK_NULL_HANDLE;
    };

    std::vector<FrameSync> mFrames;
    uint32_t mFrameIndex = 0;
    uint32_t mImageIndex = 0;

    // recreate
    bool mNeedRecreateSwapchain { false };
};

} // namespace toy
