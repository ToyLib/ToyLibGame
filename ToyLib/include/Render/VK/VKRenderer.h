// Render/VK/VKRenderer.h
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

//==============================================================================
// VKRenderer
//  - Minimal Vulkan renderer: clear color only (swapchain present)
//  - Fits into IRenderer::Draw() pipeline (DrawWorldPass does clear)
//==============================================================================
class VKRenderer : public IRenderer
{
public:
    VKRenderer();
    virtual ~VKRenderer();

    bool Initialize(const Application* app) override;
    void Shutdown() override;
    void WaitIdle() override;

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

private:
    //--------------------------------------------------------------------------
    // Init steps
    //--------------------------------------------------------------------------
    bool CreateInstance(SDL_Window* window);
    bool CreateDebugMessenger();
    bool CreateSurface(SDL_Window* window);

    bool PickPhysicalDevice();
    bool CreateDeviceAndQueues();

    bool CreateSwapchain();
    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffers();

    bool CreateCommandPool();
    bool CreateCommandBuffers();

    bool CreateSyncObjects();

    //--------------------------------------------------------------------------
    // Runtime
    //--------------------------------------------------------------------------
    bool RecordCommandBuffer(uint32_t imageIndex);
    bool Present(uint32_t imageIndex);

    bool RecreateSwapchain();
    void CleanupSwapchain();

    //--------------------------------------------------------------------------
    // Destroy helpers
    //--------------------------------------------------------------------------
    void DestroyDeviceRelated();
    void DestroyInstanceRelated();

private:
    //--------------------------------------------------------------------------
    // Core handles
    //--------------------------------------------------------------------------
    VkInstance               mInstance  = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMsgr = VK_NULL_HANDLE;
    VkSurfaceKHR             mSurface   = VK_NULL_HANDLE;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    VkQueue  mGraphicsQueue  = VK_NULL_HANDLE;
    VkQueue  mPresentQueue   = VK_NULL_HANDLE;
    uint32_t mGraphicsFamily = 0;
    uint32_t mPresentFamily  = 0;

    //--------------------------------------------------------------------------
    // Swapchain
    //--------------------------------------------------------------------------
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;

    VkFormat        mSwapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D      mSwapchainExtent {};
    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    //--------------------------------------------------------------------------
    // Render pass / FB
    //--------------------------------------------------------------------------
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mFramebuffers;

    //--------------------------------------------------------------------------
    // Commands
    //--------------------------------------------------------------------------
    VkCommandPool mCmdPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCmdBuffers; // one per swapchain image

    //--------------------------------------------------------------------------
    // Sync (frames in flight)
    //--------------------------------------------------------------------------
    static constexpr int kMaxFramesInFlight = 2;

    std::vector<VkSemaphore> mImageAvailable;
    std::vector<VkSemaphore> mRenderFinished;
    std::vector<VkFence>     mInFlightFences;

    std::vector<VkFence>     mImagesInFlight; // per swapchain image

    uint32_t mFrameIndex = 0;

    //--------------------------------------------------------------------------
    // Frame state
    //--------------------------------------------------------------------------
    bool     mFrameBegan   = false;
    uint32_t mImageIndex   = 0;

    // resize / recreate
    bool mFramebufferResized = false;

    // keep window pointer if you want (optional)
    SDL_Window* mSDLWindow = nullptr;
};

} // namespace toy
