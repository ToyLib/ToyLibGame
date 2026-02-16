#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/RenderHandles.h"

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

struct PushConstants_Mesh
{
    Matrix4 pcWorld;

    float pcDiffuse[4];
    float pcUniform[4];
    float pcFlagsSpec[4];
};

struct SpritePush
{
    float world[16];
    float colorAlpha[4];
};


struct SpriteDescCacheEntry
{
    std::vector<VkDescriptorSet> sets; // swapchain枚数分
    VkImageView lastView    { VK_NULL_HANDLE };
    VkSampler   lastSampler { VK_NULL_HANDLE };
};



struct FrameSync
{
    VkSemaphore     imageAvailable { VK_NULL_HANDLE };
    VkSemaphore     renderFinished { VK_NULL_HANDLE };
    VkFence         inFlight       { VK_NULL_HANDLE };
    VkCommandBuffer cmd            { VK_NULL_HANDLE };
};

//==============================================================
// WorldFrameResources
//  - swapchain image index (= mImageIndex) 単位で保持
//  - set=1 の UBO 0..3 と、それを参照する descriptor set
//==============================================================
struct WorldFrameResources
{
    VkDescriptorSet descSet1_Common { VK_NULL_HANDLE };

    // binding=0 : WorldCommon
    VkBuffer       worldCommonUBO { VK_NULL_HANDLE };
    VkDeviceMemory worldCommonMem { VK_NULL_HANDLE };

    // binding=1 : DirLight
    VkBuffer       dirLightUBO { VK_NULL_HANDLE };
    VkDeviceMemory dirLightMem { VK_NULL_HANDLE };

    // binding=2 : PointLight
    VkBuffer       pointLightUBO { VK_NULL_HANDLE };
    VkDeviceMemory pointLightMem { VK_NULL_HANDLE };
};

//==============================================================
// SpriteFrameResources (swapchain image 単位)
//  - set=1 binding=0 : SpriteCommon(viewProj)
//==============================================================
struct SpriteFrameResources
{
    VkDescriptorSet descSet1_SpriteCommon { VK_NULL_HANDLE };
    VkBuffer        spriteCommonUBO      { VK_NULL_HANDLE };
    VkDeviceMemory  spriteCommonMem      { VK_NULL_HANDLE };
};

class VKRenderer final : public IRenderer
{
public:
    VKRenderer();
    ~VKRenderer() override { Shutdown(); }

    bool Initialize(const class Application* app) override;
    void Shutdown() override;

    void UnloadData() override {}
    void OnWindowResized(int /*pixelW*/, int /*pixelH*/) override {}

    PipelineHandle GetPipelineHandle(const std::string& name) override;

    void SetClearColor(const Vector3& color) override { mClearColor = color; }
    std::shared_ptr<class IRenderTarget> CreateRenderTarget() override { return nullptr; }

    VkCommandBuffer GetActiveCommandBuffer() const
    {
        if (mFrames.empty() || mFrameIndex >= (uint32_t)mFrames.size())
        {
            return VK_NULL_HANDLE;
        }
        return mFrames[mFrameIndex].cmd;
    }

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
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override {}
    void DrawFadePass() override {}
    void DrawPostEffectPass() override {}
    void DrawUIPass() override;

private:
    //--------------------------------------------------------------------------
    // Core VK init
    //--------------------------------------------------------------------------
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandBuffers();
    void DestroyFramebuffers();

    //--------------------------------------------------------------------------
    // Pipelines
    //--------------------------------------------------------------------------
    bool CreateSpritePipeline();
    bool CreateMeshPipeline();
    bool CreateSkinnedMeshPipeline();

    PipelineHandle FindPipelineHandle(const std::string& name) const;

    //--------------------------------------------------------------------------
    // World draw helpers
    //--------------------------------------------------------------------------
    void DrawBucket_WorldVK(const std::vector<uint32_t>& bucket);
    void DrawWorldItem_VK(const RenderItem& it);
    VKPipeline* ResolveWorldPipelineForItem(const RenderItem& it);

    //--------------------------------------------------------------------------
    // World bind helpers
    //--------------------------------------------------------------------------
    void BindWorldCommon(VkCommandBuffer cmd,
                         const VKPipeline& pipe,
                         const RenderItem& it);

    void BindWorldMaterial(VkCommandBuffer cmd,
                           const VKPipeline& pipe,
                           const RenderItem& it);

private:
    //--------------------------------------------------------------------------
    // Sprite (既存のまま)
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

    bool EnsureSpriteGeometryVK();
    void DestroySpriteGeometryVK();

    std::unordered_map<const class VertexArray*, VKGeometry> mSpriteGeoVK;

    bool EnsureSpriteDescriptorPool();
    VkDescriptorSet GetOrCreateSpriteDescSet(TextureHandle tex);

    VkDescriptorPool mSpriteDescPool { VK_NULL_HANDLE };
    //std::unordered_map<const class Texture*, std::vector<VkDescriptorSet>> mSpriteDescSetsVK;
    std::unordered_map<const class Texture*, SpriteDescCacheEntry> mSpriteDescSetsVK;

    VkImageView GetVkImageViewFromTextureHandle(TextureHandle h) const;
    VkSampler   GetVkSamplerFromTextureHandle(TextureHandle h) const;

    VkImageView mSpriteFallbackImageView { VK_NULL_HANDLE };
    VkSampler   mSpriteFallbackSampler   { VK_NULL_HANDLE };
    
    //--------------------------------------------------------------------------
    // Sprite common (set=1) : viewProj UBO
    //--------------------------------------------------------------------------
    VkDescriptorSetLayout mSpriteSetLayout1_Common { VK_NULL_HANDLE };
    VkDescriptorPool      mSpriteCommonDescPool   { VK_NULL_HANDLE };
    std::vector<SpriteFrameResources> mSpriteFrames;

    bool EnsureSpriteCommonDescriptors();
    void DestroySpriteCommonDescriptors();
    void UpdateSpriteCommonUBO(uint32_t imageIndex);
    

private:
    // World set layouts (共有)
    //  - set=0 : texture sampler
    //  - set=1 : UBO 0..2 (WorldCommon / DirLight / PointLight)
    VkDescriptorSetLayout mWorldSetLayout0_Texture { VK_NULL_HANDLE };
    VkDescriptorSetLayout mWorldSetLayout1_Common  { VK_NULL_HANDLE };

private:
    //--------------------------------------------------------------------------
    // World descriptors / UBOs (set=1)
    //--------------------------------------------------------------------------
    VkDescriptorPool mWorldDescPool { VK_NULL_HANDLE };

    // swapchain image index 単位
    std::vector<WorldFrameResources> mWorldFrames;

    bool EnsureWorldDescriptors();
    void DestroyWorldDescriptors();

    // Update: 반드시 imageIndex で対象を決める
    void UpdateWorldCommonUBO(uint32_t imageIndex);
    void UpdateDirLightUBO(uint32_t imageIndex);
    void UpdatePointLightUBO(uint32_t imageIndex);

    // set0 (Diffuse) : Texture -> descriptor
    VkDescriptorSet GetOrCreateWorldTexDescSet(TextureHandle texH);

    // Dummy white (no texture fallback)
    bool CreateDummyWhiteResources();
    void DestroyDummyWhiteResources();

private:
    //--------------------------------------------------------------------------
    // Upload helpers
    //--------------------------------------------------------------------------
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    bool CreateBuffer(VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props,
                      VkBuffer& outBuf,
                      VkDeviceMemory& outMem);

    bool BeginOneShot(VkCommandBuffer& outCmd);
    void EndOneShot(VkCommandBuffer cmd);

    void TransitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout);

    void CopyBufferToImage(VkCommandBuffer cmd,
                           VkBuffer buffer,
                           VkImage image,
                           uint32_t width,
                           uint32_t height);

    bool CreateHostVisibleUBO(VkPhysicalDevice phys,
                              VkDevice device,
                              VkDeviceSize size,
                              VkBuffer& outBuf,
                              VkDeviceMemory& outMem);

private:
    //--------------------------------------------------------------------------
    // SDL
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

    //--------------------------------------------------------------------------
    // set0 (World Diffuse) descriptor pool + cache
    //--------------------------------------------------------------------------
    VkDescriptorPool mWorldTexDescPool { VK_NULL_HANDLE };
    std::unordered_map<const class Texture*, VkDescriptorSet> mWorldTexDescSetCache;
    VkDescriptorSet mWorldTexDescSetDummyWhite { VK_NULL_HANDLE };

    // Dummy white resources
    VkImage        mDummyWhiteImage { VK_NULL_HANDLE };
    VkDeviceMemory mDummyWhiteMemory { VK_NULL_HANDLE };
    VkImageView    mDummyWhiteImageView { VK_NULL_HANDLE };
    VkSampler      mDummyWhiteSampler { VK_NULL_HANDLE };
};

} // namespace toy
