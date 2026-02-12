#pragma once

#include "Render/IRenderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace toy
{

//==============================================================
// FrameSync（最低限のフレーム同期）
//==============================================================
struct FrameSync
{
    VkSemaphore     imageAvailable { VK_NULL_HANDLE };
    VkSemaphore     renderFinished { VK_NULL_HANDLE };
    VkFence         inFlight       { VK_NULL_HANDLE };
    VkCommandBuffer cmd            { VK_NULL_HANDLE };
};

//==============================================================
// VKRenderer（最小：Clear + Present まで）
//==============================================================
class VKRenderer final : public IRenderer
{
public:
    VKRenderer() = default;
    ~VKRenderer() override { Shutdown(); }

    //--------------------------------------------------------------------------
    // IRenderer
    //--------------------------------------------------------------------------
    bool Initialize(const class Application* app) override;
    void Shutdown() override;

    void UnloadData() override {}
    void OnWindowResized(int /*pixelW*/, int /*pixelH*/) override {}

    std::shared_ptr<class Shader> GetShader(const std::string& /*name*/) { return nullptr; }
    PipelineHandle GetPipelineHandle(const std::string& /*name*/) override { return {}; }

    void SetClearColor(const Vector3& color) override { mClearColor = color; }
    std::shared_ptr<RenderTarget> CreateRenderTarget() override { return nullptr; }

protected:
    void ApplyState(const RenderItem& /*it*/) override {}
    void DrawItem(const RenderItem& /*it*/, RenderPass /*pass*/, int /*cascadeIndex*/) override {}

    bool InitializeShadowMapping() override { return false; }
    void DrawToRenderTarget(const struct SceneCaptureRequest& /*req*/) override {}

    bool BeginFrame() override;
    void EndFrame() override;

    void DrawShadowPass() override {}
    void RestoreAfterShadowPass() override {}

    void DrawSkyPass() override {}
    void DrawWorldPass() override {}
    void DrawOverlayScreenPass() override {}
    void DrawFadePass() override {}
    void DrawPostEffectPass() override {}
    void DrawUIPass() override;

private:
    //--------------------------------------------------------------------------
    // Internal helpers (minimum)
    //--------------------------------------------------------------------------
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandBuffers(); // ★ BeginFrame の vkResetCommandBuffer の前提（後で実装）
    void DestroyFramebuffers();
    

private:
    //--------------------------------------------------------------------------
    // SDL (non-owning)
    //--------------------------------------------------------------------------
    SDL_Window* mWindow { nullptr };

    //--------------------------------------------------------------------------
    // Instance / Surface
    //--------------------------------------------------------------------------
    bool                   mEnableValidation { true };
    VkInstance             mInstance         { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT mDebugMessenger { VK_NULL_HANDLE };
    VkSurfaceKHR           mSurface          { VK_NULL_HANDLE };

    //--------------------------------------------------------------------------
    // Device / Queues
    //--------------------------------------------------------------------------
    VkPhysicalDevice             mPhysicalDevice     { VK_NULL_HANDLE };
    VkDevice                     mDevice             { VK_NULL_HANDLE };
    VkQueue                      mQueueGraphics      { VK_NULL_HANDLE };
    VkQueue                      mQueuePresent       { VK_NULL_HANDLE };
    uint32_t                     mQueueFamilyGraphics { UINT32_MAX };
    uint32_t                     mQueueFamilyPresent  { UINT32_MAX };
    std::vector<const char*>     mDeviceExtensions; // "VK_KHR_swapchain", etc.

    //--------------------------------------------------------------------------
    // Swapchain
    //--------------------------------------------------------------------------
    VkSwapchainKHR               mSwapchain         { VK_NULL_HANDLE };
    VkExtent2D                   mSwapchainExtent   {};
    VkSurfaceFormatKHR           mSwapchainFormat   {};
    VkPresentModeKHR             mPresentMode       { VK_PRESENT_MODE_FIFO_KHR };

    std::vector<VkImage>         mSwapchainImages;
    std::vector<VkImageView>     mSwapchainImageViews;
    
    bool mNeedRecreateSwapchain {true};


    //--------------------------------------------------------------------------
    // RenderPass / Framebuffers
    //--------------------------------------------------------------------------
    VkRenderPass                 mRenderPass        { VK_NULL_HANDLE };
    std::vector<VkFramebuffer>   mFramebuffers;

    //--------------------------------------------------------------------------
    // Commands / Sync
    //--------------------------------------------------------------------------
    VkCommandPool                mCommandPool       { VK_NULL_HANDLE };
    std::vector<FrameSync>       mFrames;
    uint32_t                     mFrameIndex        { 0 };
    uint32_t                     mImageIndex        { 0 };
    
    //========================
    // UI(Sprite) minimal
    //========================
    bool EnsureUIResources();
    void DestroyUIResources();

    bool mUiReady { false };

    // pipeline
    VkDescriptorSetLayout mUiSetLayout { VK_NULL_HANDLE };
    VkDescriptorPool      mUiDescPool  { VK_NULL_HANDLE };
    std::vector<VkDescriptorSet> mUiDescSets;

    VkSampler    mUiSampler { VK_NULL_HANDLE };
    VkPipelineLayout mUiPipelineLayout { VK_NULL_HANDLE };
    VkPipeline       mUiPipeline       { VK_NULL_HANDLE };

    // test texture (1x1 white)
    VkImage        mUiTestImage       { VK_NULL_HANDLE };
    VkDeviceMemory mUiTestImageMemory { VK_NULL_HANDLE };
    VkImageView    mUiTestImageView   { VK_NULL_HANDLE };

    // test quad buffers
    VkBuffer       mUiVB       { VK_NULL_HANDLE };
    VkDeviceMemory mUiVBMemory { VK_NULL_HANDLE };
    VkBuffer       mUiIB       { VK_NULL_HANDLE };
    VkDeviceMemory mUiIBMemory { VK_NULL_HANDLE };
    uint32_t       mUiIndexCount { 0 };
    
};

} // namespace toy
