#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/RenderHandles.h" // PipelineHandle / TextureHandle etc.

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
    std::shared_ptr<class IRenderTarget> CreateRenderTarget() override { return nullptr; }

    //--------------------------------------------------------------------------
    // VK helpers
    //--------------------------------------------------------------------------
    VkCommandBuffer GetActiveCommandBuffer() const
    {
        if (mFrames.empty() || mFrameIndex >= (uint32_t)mFrames.size())
        {
            return VK_NULL_HANDLE;
        }
        return mFrames[mFrameIndex].cmd;
    }

protected:
    //--------------------------------------------------------------------------
    // IRenderer protected
    //--------------------------------------------------------------------------
    void ApplyState(const RenderItem& /*it*/) override {}
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

    bool InitializeShadowMapping() override { return false; }
    void DrawToRenderTarget(const struct SceneCaptureRequest& /*req*/) override {}

    bool BeginFrame() override;
    void EndFrame() override;

    void DrawShadowPass() override {}
    void RestoreAfterShadowPass() override {}

    void DrawSkyPass() override {}
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override {}
    void DrawFadePass() override {}
    void DrawPostEffectPass() override {}
    void DrawUIPass() override;

private:
    //--------------------------------------------------------------------------
    // Core VK init (minimum)
    //--------------------------------------------------------------------------
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandBuffers();
    void DestroyFramebuffers();

    //--------------------------------------------------------------------------
    // Pipelines
    //--------------------------------------------------------------------------
    bool CreateSpritePipeline();

    // 3D
    bool CreateMeshPipeline();
    bool CreateSkinnedMeshPipeline();

    PipelineHandle FindPipelineHandle(const std::string& name) const;

    //--------------------------------------------------------------------------
    // World draw helpers
    //--------------------------------------------------------------------------
    void DrawBucket_WorldVK(const std::vector<uint32_t>& bucket);
    void DrawWorldItem_VK(const RenderItem& it);

private:
    //--------------------------------------------------------------------------
    // SpriteQueue(UI bucket) rendering helpers
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
    bool EnsureSpriteGeometryVK();
    void DestroySpriteGeometryVK();

    std::unordered_map<const class VertexArray*, VKGeometry> mSpriteGeoVK;

    // (B) DescriptorSet: Texture* -> (swapchain枚数分の VkDescriptorSet)
    bool EnsureSpriteDescriptorPool();
    VkDescriptorSet GetOrCreateSpriteDescSet(TextureHandle tex);

    VkDescriptorPool mSpriteDescPool { VK_NULL_HANDLE };
    std::unordered_map<const class Texture*, std::vector<VkDescriptorSet>> mSpriteDescSetsVK;

    // (C) TextureHandle -> Vulkan resource bridge
    VkImageView GetVkImageViewFromTextureHandle(TextureHandle h) const;
    VkSampler   GetVkSamplerFromTextureHandle(TextureHandle h) const;

    // Fallback for missing texture-vk bridge
    VkImageView mSpriteFallbackImageView { VK_NULL_HANDLE };
    VkSampler   mSpriteFallbackSampler   { VK_NULL_HANDLE };

private:
    //--------------------------------------------------------------------------
    // Scene(Common) descriptor resources (set=1)
    //--------------------------------------------------------------------------
    VkDescriptorPool              mWorldDescPool { VK_NULL_HANDLE };
    std::vector<VkDescriptorSet>  mWorldDescSets; // swapchain枚数ぶん

    // UBO: binding0 WorldCommon
    VkBuffer       mWorldCommonUBO { VK_NULL_HANDLE };
    VkDeviceMemory mWorldCommonUBOMem { VK_NULL_HANDLE };

    // UBO: binding3 MaterialParams
    VkBuffer       mMaterialParamsUBO { VK_NULL_HANDLE };
    VkDeviceMemory mMaterialParamsUBOMem { VK_NULL_HANDLE };

    // UBO: binding4 DirLight
    VkBuffer       mDirLightUBO { VK_NULL_HANDLE };
    VkDeviceMemory mDirLightUBOMem { VK_NULL_HANDLE };

    // UBO: binding5 PointLights
    VkBuffer       mPointLightUBO { VK_NULL_HANDLE };
    VkDeviceMemory mPointLightUBOMem { VK_NULL_HANDLE };

    // 生成/破棄
    bool EnsureWorldDescriptors();      // pool + sets + UBO + update
    void DestroyWorldDescriptors();     // Shutdownで

    // フレーム毎に更新
    void UpdateWorldCommonUBO(uint32_t imageIndex);
    void UpdateMaterialParamsUBO(const RenderItem& it);
    void UpdateDirLightUBO();
    void UpdatePointLightUBO();

    // Draw時にbind（Mesh用）
    void BindWorldCommon(VkCommandBuffer cmd,
                         const VKPipeline& pipe,
                         const RenderItem& it);
    void BindWorldMaterial(VkCommandBuffer cmd,
                           const VKPipeline& p,
                           const RenderItem& it);

private:
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
    // Pipelines storage
    //--------------------------------------------------------------------------
    std::unordered_map<std::string, std::unique_ptr<VKPipeline>> mPipelines;
    
    // -----------------------------------------------------------------------------
    // Dummy shadow resources (temporary until real shadow mapping)
    //  - Used to satisfy shader bindings when shadow is not implemented.
    // -----------------------------------------------------------------------------
    VkImage        mDummyShadowImage      { VK_NULL_HANDLE };
    VkDeviceMemory mDummyShadowMem        { VK_NULL_HANDLE };
    VkImageView    mDummyShadowImageView  { VK_NULL_HANDLE };
    VkSampler      mDummyShadowSampler    { VK_NULL_HANDLE };

};

} // namespace toy
