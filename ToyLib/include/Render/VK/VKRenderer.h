#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/VKPipeline.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

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
// VKRenderer
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

    PipelineHandle GetPipelineHandle(const std::string& name) override;

    void SetClearColor(const Vector3& color) override { mClearColor = color; }
    std::shared_ptr<RenderTarget> CreateRenderTarget() override { return nullptr; }

    // Pipelines (name -> VKPipeline)
    std::unordered_map<std::string, std::unique_ptr<VKPipeline>> mPipelines;

protected:
    void ApplyState(const RenderItem& /*it*/) override {}
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

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
    bool CreateCommandBuffers();
    void DestroyFramebuffers();

    bool CreateSpritePipeline();
    

private:
    //--------------------------------------------------------------------------
    // SpriteQueue(UI bucket) rendering helpers
    //  - VKRenderer_DrawUI.cpp（あなたが貼った版）に合わせた宣言
    //--------------------------------------------------------------------------
    struct VKGeometry
    {
        VkBuffer       vb { VK_NULL_HANDLE };
        VkDeviceMemory vbMem { VK_NULL_HANDLE };
        VkBuffer       ib { VK_NULL_HANDLE };
        VkDeviceMemory ibMem { VK_NULL_HANDLE };
        uint32_t       indexCount { 0 };
        VkDeviceSize   vbBytes { 0 };
        VkDeviceSize   ibBytes { 0 };
    };

    // (A) Geometry: VertexArray* -> VK VB/IB cache
    bool EnsureSpriteGeometryVK();  // IRenderer::GetSpriteQuad() を VK VB/IB 化
    void DestroySpriteGeometryVK(); // Shutdown で呼ぶ用（任意だが用意推奨）

    std::unordered_map<const class VertexArray*, VKGeometry> mSpriteGeoVK;

    // (B) DescriptorSet: Texture* -> (swapchain枚数分の VkDescriptorSet)
    bool EnsureSpriteDescriptorPool();                 // pool 作成（lazily）
    VkDescriptorSet GetOrCreateSpriteDescSet(TextureHandle tex); // そのフレームの set を返す

    VkDescriptorPool mSpriteDescPool { VK_NULL_HANDLE };
    std::unordered_map<const class Texture*, std::vector<VkDescriptorSet>> mSpriteDescSetsVK;

    // (C) TextureHandle -> Vulkan resource bridge
    VkImageView GetVkImageViewFromTextureHandle(TextureHandle h) const;
    VkSampler   GetVkSamplerFromTextureHandle(TextureHandle h) const;

    // Fallback (texture が無い/未対応の時に使う)
    VkImageView mSpriteFallbackImageView { VK_NULL_HANDLE };
    VkSampler   mSpriteFallbackSampler   { VK_NULL_HANDLE };
    
    bool mUiImageLayoutReady { false };

    //--------------------------------------------------------------------------
    // SDL (non-owning)
    //--------------------------------------------------------------------------
    SDL_Window* mWindow { nullptr };

    //--------------------------------------------------------------------------
    // Instance / Surface
    //--------------------------------------------------------------------------
    bool                     mEnableValidation { true };
    VkInstance               mInstance         { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT mDebugMessenger   { VK_NULL_HANDLE };
    VkSurfaceKHR             mSurface          { VK_NULL_HANDLE };

    //--------------------------------------------------------------------------
    // Device / Queues
    //--------------------------------------------------------------------------
    VkPhysicalDevice         mPhysicalDevice      { VK_NULL_HANDLE };
    VkDevice                 mDevice              { VK_NULL_HANDLE };
    VkQueue                  mQueueGraphics       { VK_NULL_HANDLE };
    VkQueue                  mQueuePresent        { VK_NULL_HANDLE };
    uint32_t                 mQueueFamilyGraphics { UINT32_MAX };
    uint32_t                 mQueueFamilyPresent  { UINT32_MAX };
    std::vector<const char*> mDeviceExtensions;

    //--------------------------------------------------------------------------
    // Swapchain
    //--------------------------------------------------------------------------
    VkSwapchainKHR           mSwapchain       { VK_NULL_HANDLE };
    VkExtent2D               mSwapchainExtent {};
    VkSurfaceFormatKHR       mSwapchainFormat {};
    VkPresentModeKHR         mPresentMode     { VK_PRESENT_MODE_FIFO_KHR };

    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    bool mNeedRecreateSwapchain { true };

    //--------------------------------------------------------------------------
    // RenderPass / Framebuffers
    //--------------------------------------------------------------------------
    VkRenderPass               mRenderPass  { VK_NULL_HANDLE };
    std::vector<VkFramebuffer> mFramebuffers;

    //--------------------------------------------------------------------------
    // Commands / Sync
    //--------------------------------------------------------------------------
    VkCommandPool          mCommandPool { VK_NULL_HANDLE };
    std::vector<FrameSync> mFrames;
    uint32_t               mFrameIndex { 0 };
    uint32_t               mImageIndex { 0 };

    //--------------------------------------------------------------------------
    // Optional: 旧「白Quadテスト」用（残すなら独立させる）
    //  ※SpriteQueueへ完全移行できたら削除推奨
    //--------------------------------------------------------------------------
    bool EnsureUIResources();   // 任意
    void DestroyUIResources();  // 任意

    bool mUiReady { false };

    
    VkDescriptorSetLayout        mUiSetLayout { VK_NULL_HANDLE };
    VkDescriptorPool             mUiDescPool  { VK_NULL_HANDLE };
    std::vector<VkDescriptorSet> mUiDescSets;

    VkSampler        mUiSampler        { VK_NULL_HANDLE };
    VkPipelineLayout mUiPipelineLayout { VK_NULL_HANDLE };
    VkPipeline       mUiPipeline       { VK_NULL_HANDLE };

    VkImage        mUiTestImage       { VK_NULL_HANDLE };
    VkDeviceMemory mUiTestImageMemory { VK_NULL_HANDLE };
    VkImageView    mUiTestImageView   { VK_NULL_HANDLE };

    VkBuffer       mUiVB       { VK_NULL_HANDLE };
    VkDeviceMemory mUiVBMemory { VK_NULL_HANDLE };
    VkBuffer       mUiIB       { VK_NULL_HANDLE };
    VkDeviceMemory mUiIBMemory { VK_NULL_HANDLE };
    uint32_t       mUiIndexCount { 0 };
    
    
};

} // namespace toy
