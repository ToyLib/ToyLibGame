//==============================================================================
// VKRenderer.h
//  - New VKRenderer aligned with rewritten cpp
//==============================================================================

#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/VKPipeline.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <unordered_map>

namespace toy
{

//==============================================================================
// VKRenderer
//==============================================================================
class VKRenderer : public IRenderer
{
public:
    VKRenderer();
    virtual ~VKRenderer() = default;

    bool Initialize(const Application* app) override;
    void Shutdown() override;
    void WaitIdle() override;

    PipelineHandle GetPipelineHandle(const std::string& name) override;

    void OnWindowResized(int pixelW, int pixelH) override {}

protected:
    // IRenderer phases
    bool BeginFrame() override;

    void DrawShadowPass() override {}
    void RestoreAfterShadowPass() override {}

    void DrawSkyPass() override {}
    void DrawWorldPass() override {}
    void DrawOverlayScreenPass() override {}
    void DrawFadePass() override {}
    void DrawPostEffectPass() override {}
    void DrawUIPass() override {}

    void EndFrame() override;

    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

private:
    //--------------------------------------------------------------------------
    // FrameSync
    //--------------------------------------------------------------------------
    struct FrameSync
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;

        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence     inFlight       = VK_NULL_HANDLE;
    };

private:
    bool mEnableValidation { false };

    //--------------------------------------------------------------------------
    // Core handles
    //--------------------------------------------------------------------------
    VkInstance               mInstance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             mSurface        = VK_NULL_HANDLE;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    VkQueue  mQueueGraphics = VK_NULL_HANDLE;
    VkQueue  mQueuePresent  = VK_NULL_HANDLE;

    uint32_t mQueueFamilyGraphics = UINT32_MAX;
    uint32_t mQueueFamilyPresent  = UINT32_MAX;

    std::vector<const char*> mDeviceExtensions;

    //--------------------------------------------------------------------------
    // Swapchain
    //--------------------------------------------------------------------------
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;

    VkSurfaceFormatKHR mSwapchainFormat {};
    VkPresentModeKHR   mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D         mSwapchainExtent {};

    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    //--------------------------------------------------------------------------
    // Render pass / FB
    //--------------------------------------------------------------------------
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mFramebuffers;

    //--------------------------------------------------------------------------
    // Commands / sync
    //--------------------------------------------------------------------------
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<FrameSync> mFrames;

    uint32_t mFrameIndex = 0;
    uint32_t mImageIndex = 0;

    bool mFrameBegan = false;

    // swapchain recreate
    bool mNeedRecreateSwapchain = false;

    //--------------------------------------------------------------------------
    // Pipelines
    //--------------------------------------------------------------------------
    std::unordered_map<std::string, std::unique_ptr<class VKPipeline>> mPipelines;

private:
    // swapchain dependent
    bool CreateRenderPass();
    bool CreateFramebuffers();

    // pipelines (existing functions from your cpp)
    //bool CreateSpritePipeline();
    //bool CreateMeshPipeline();
    //bool CreateSkinnedMeshPipeline();

    // dummy resources (existing)
    //bool CreateDummyWhiteResources();
    //void DestroyDummyWhiteResources();
};

} // namespace toy
