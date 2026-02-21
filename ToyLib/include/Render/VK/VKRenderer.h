//======================================================================
// Render/VK/VKRenderer.h  (整理版：宣言をCore/Drawpass分割に合わせて統一)
//  - swapchain depth は CreateDepthForSwapchain/DestroyDepthForSwapchain のみ
//  - CreateDepthResources/DestroyDepthResources は廃止
//  - ChooseDepthFormat/FindMemoryType/CreateImage2D/CreateImageView2D は vkutil に寄せる
//======================================================================
#pragma once

#include "Render/IRenderer.h"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace toy
{

class Application;

struct RenderItem;
enum class RenderPass;

class VKRenderer final : public IRenderer
{
public:
    VKRenderer();
    ~VKRenderer() override;

    bool Initialize(const Application* app) override;
    void Shutdown() override;

    void WaitIdle() override;

    std::shared_ptr<IRenderTarget> CreateRenderTarget() override;
    void DrawToRenderTarget(const SceneCaptureRequest& req) override;

    void OnWindowResized(int pixelW, int pixelH) override;

    bool BeginFrame() override;
    void EndFrame() override;

    // draw phases
    void DrawShadowPass() override;
    void RestoreAfterShadowPass() override;
    void DrawSkyPass() override;
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override;
    void DrawFadePass() override;
    void DrawPostEffectPass() override;
    void DrawUIPass() override;

    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

    PipelineHandle GetPipelineHandle(const std::string& name) override;

private:
    //==============================================================
    // Core: init steps
    //==============================================================
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateDeviceAndQueues();
    bool CreateSwapchainAndViews();

    bool CreateDepthForSwapchain();
    void DestroyDepthForSwapchain();

    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();

    bool RecreateSwapchain();
    void CleanupSwapchain();

    // one-time command helpers (安全版：submitだけ待つ想定に拡張しやすい)
    VkCommandBuffer BeginOneTimeCommands();
    void EndOneTimeCommands(VkCommandBuffer cmd);
    
    
    std::string MakeVKSpvPath(const std::string& filename) const;
    VkShaderModule LoadShaderModule(const std::string& spvFile);

private:
    //==============================================================
    // Per-frame sync
    //==============================================================
    struct FrameSync
    {
        VkCommandBuffer cmd           = VK_NULL_HANDLE;
        VkSemaphore     imageAvailable= VK_NULL_HANDLE;
        VkSemaphore     renderFinished= VK_NULL_HANDLE;
        VkFence         inFlight      = VK_NULL_HANDLE;
    };

private:
    //==============================================================
    // Vulkan core handles
    //==============================================================
    VkInstance       mInstance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

    VkSurfaceKHR     mSurface        = VK_NULL_HANDLE;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    VkQueue          mQueueGraphics  = VK_NULL_HANDLE;
    VkQueue          mQueuePresent   = VK_NULL_HANDLE;
    uint32_t         mQueueFamilyGraphics = UINT32_MAX;
    uint32_t         mQueueFamilyPresent  = UINT32_MAX;

    VkSwapchainKHR               mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage>         mSwapchainImages;
    std::vector<VkImageView>     mSwapchainImageViews;
    VkSurfaceFormatKHR           mSwapchainFormat{};
    VkExtent2D                   mSwapchainExtent{};

    // swapchain render pass / FB
    VkRenderPass                 mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer>   mFramebuffers;

    // swapchain depth (single shared depth)
    VkFormat       mDepthFormat   = VK_FORMAT_UNDEFINED;
    VkImage        mDepthImage    = VK_NULL_HANDLE;
    VkDeviceMemory mDepthMemory   = VK_NULL_HANDLE;
    VkImageView    mDepthImageView= VK_NULL_HANDLE;

    // command pool + per-frame command buffers
    VkCommandPool              mCommandPool = VK_NULL_HANDLE;
    std::vector<FrameSync>     mFrames;
    uint32_t                   mFrameIndex = 0;
    uint32_t                   mImageIndex = 0;

    // state
    bool   mEnableValidation = true;
    bool   mNeedRecreateSwapchain = false;

    // IRenderer の mWindow を使う前提（protected）
    // SDL_Window* mWindow;  // IRenderer 側

    // geometry (IRenderer が持つ shared_ptr などに合わせている前提)
    // mFullScreenQuad / mSpriteQuad / mSurfaceQuad は IRenderer 側メンバを利用

    // window scale
    float  mWindowDisplayScale = 1.0f;

private:
    // Triangle
    bool CreatePipelineLayout_Triangle();
    bool CreatePipeline_Triangle();
    VkPipelineLayout mPipeLayoutTriangle = VK_NULL_HANDLE;
    VkPipeline mPipelineTriangle = VK_NULL_HANDLE;
};

} // namespace toy
